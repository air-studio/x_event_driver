/*
 * io_procedure.c
 *
 *  Created on: Jan 18, 2017
 *      Author: xpc
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/aio_abi.h>
#include <sys/syscall.h>
#include <sys/eventfd.h>
#include "include/io_procedure.h"

/*初始化*/
int ioInit(aio_context_t* ctx, unsigned eventQueueCapacity,EventWorker * ew) {
	memset(ctx,0, sizeof(aio_context_t));  // It's necessary，这里一定要的
	/*Syscall IO_setup*/
	if (syscall(SYS_io_setup, eventQueueCapacity, *ctx))
		printf("io_setup success\n");
	else {
		perror("io_setup error\n");
		quick_exit(-1);
	}
	for(int i=0;i<ew->workers;i++)
		ew->efd[i]=eventfd(0, 0);
	return 0;
}

/*提交IO请求*/
int ioSubmit(EventWorker* ew,EventProc * ep,int fd,unsigned io_cmd,long int req_prio,__u64 buf,__s64 offset) {
    struct iocb iocb,*p=&iocb;
    memset(p,0,sizeof(iocb));

    iocb.aio_data       = (__u64)ep;
    iocb.aio_lio_opcode = io_cmd;//IO_CMD_PWRITE; //
    iocb.aio_reqprio    = req_prio;//sysconf(_SC_ARG_MAX);
    iocb.aio_fildes     = fd;
    iocb.aio_buf    = (__u64)buf;
    iocb.aio_nbytes = sysconf(_SC_PAGESIZE);//这个值必须按page字节对齐
    iocb.aio_offset = offset; // 这个值必须按page字节对齐

    // 提交异步操作，异步写磁盘
    int n = syscall(SYS_io_submit,ew->ctx, 1, &p);
    if(n<0)perror("==io_submit==: %d:%s\n");
	return 0;
}

static int RUN_FLAG=1;//控制记号
int dispatch(EventWorker* ew, const struct io_event *ar, const int n);
/*监听IO完成事件*/
int ioCompleteEventListen(EventWorker* ew,unsigned get_min,unsigned get_max,struct timespec waitTimeout,struct io_event *events) {
    if(get_min>get_max)
    	perror("get_min>get_max");
    else if(get_min<1||get_max<1)
    	perror("get_min<1||get_max<1");

    int n;
    unsigned min=get_min,max=get_max;
    struct timespec *timeout=&waitTimeout;

	//批处理策略，等待时间配合等待数量，动态调节，适应不同数量级的吞吐压力
	//初始策略：
	do{
		n = syscall(SYS_io_getevents,ew->ctx, min, max, events, timeout);
	    if(n<0) perror("io_getevents: %d:%s\n");
	    else if(n<1){
	    	timeout=NULL;
	    	min=1;
	    }else if(min==1){
	    	min=get_min;
	    	timeout=&waitTimeout;
	    }
	    //分派接收到的完成事件
	    dispatch(ew, events, n);
	}while(RUN_FLAG);

	return 0;
}

/*处理已完成事件*/
int onIOComplete(EventWorker* ew,unsigned short workerId) {
	uint64_t n=0;
	int* h=ew->head+workerId, cap=ew->singleCap;
	eventfd_read(ew->efd[workerId], &n);
	while(n--){
		EventProc* ep=(EventProc*)((struct io_event*)(ew->queue+workerId*cap+*h))->data;
		ep->callback(ep->data);
		*h=(1+*h)%cap;
	}
	return 0;
}


const int* par;
int compLeftSortIndex(const void* p1, const void* p2) 
{
	return par[*(int*)p1] < par[*(int*)p2]?1:-1;
}
/*多路分派策略，按剩余可处理容量，均衡分派*/
int dispatch(EventWorker* ew, const struct io_event *ar, const int n){
	int nn=n;
//算left
	for(int i=0;i<ew->workers;++i){
		ew->left[i]=(ew->left[i]=ew->end[i]-ew->head[i])<0?-ew->left[i]:ew->singleCap-ew->left[i];
	}
//按left排序，得出排序后的index序列
	par=ew->left;
	qsort(ew->indexArray,ew->workers,sizeof(int),&compLeftSortIndex);
//填坑算法，从最大缺口开始填。计算出水位线。
	int i,pu,pui;
	for(i=1;i<ew->workers;++i){
		int x=ew->left[ew->indexArray[i]]-ew->left[ew->indexArray[i-1]];
		if(x>0){
			if(nn-x*i<=0){
				pu=ew->left[ew->indexArray[i]]-nn/i;
				pui=nn%i;
				break;
			}else
				nn-=x*i;
		}
	}

	if(i>=ew->workers){
		int x=ew->left[ew->indexArray[ew->workers-1]];
		if(nn-x*ew->workers<=0){
			pu=x-nn/i;
			pui=nn%i;				
		}else{
			pu=0;
			pui=0;
			//放不下，报错
			printf("out of queue: %d event dropped!",nn-x*ew->workers);
		}
		--i;
	}
	//按水位线copy
	nn=0;
	for(int j=0;j<i;++j){
		int in=j<pui?ew->left[ew->indexArray[i]]+1-pu:ew->left[ew->indexArray[i]]-pu;
		if(in>0){
			int tmp=ew->singleCap-ew->end[j];
			if(in-tmp<=0)
				memcpy(((struct io_event*)ew->queue)+j*ew->singleCap+ew->end[j],ar+nn,sizeof(struct io_event)*in);
			else{
				memcpy(((struct io_event*)ew->queue)+j*ew->singleCap+ew->end[j],ar+nn,sizeof(struct io_event)*tmp);
				memcpy(((struct io_event*)ew->queue)+j*ew->singleCap,ar+nn+tmp,sizeof(struct io_event)*(in-tmp));
			}
			ew->end[j]=(ew->end[j]+in)%ew->singleCap;
			nn+=in;
			//通知处理线程,eventfd++;
			if(eventfd_write(ew->efd[j],in)<0){
				perror("eventdf error!"); exit(EXIT_FAILURE);
			}
		}else
			break;
	}
	return 0;
}


