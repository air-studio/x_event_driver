/*
 * aios.h
 *
 *  Created on: Jul 12, 2016
 *      Author: xpc
 */

#ifndef SRC_AIOS_AIOS_H_
#define SRC_AIOS_AIOS_H_
#include <linux/aio_abi.h>
#include <sys/syscall.h>


typedef struct EVENT_PROC{
	void* data;
	void(*callback)(void*);
}EventProc;


struct aios{
	aio_context_t ctx;
	int cb_workers;		//callback processors
	int cb_array_cap;	//[each callback process array length]-1;
	int* H_E;		//if H=E, used=0; eles if H=(E+1)%(cb_array_cap+1), used up
	struct io_event *events;//ev array length= cb_workers*(cb_array_cap+1)
};

#define AIOS_INIT(a,workers,each_array_cap)\
	a=(struct aios){\
		.cb_workers=workers,\
    		.cb_array_cap=each_array_cap,\
		.events=(struct io_event *)malloc((each_array_cap+1)*workers*sizeof(struct io_event)),\
		.H_E=malloc(2*workers*sizeof(int))\
	};\
	memset(&(a.ctx),0,sizeof(a.ctx));\
	memset(a.H_E,0,sizeof(int)*2*workers);

#define AIOS_SETUP(a)\
	syscall(SYS_io_setup, a.cb_array_cap*workers, &(a.ctx))

#define AIOS_SUBMITE(a,iocbs_p,nr)\
	syscall(SYS_io_submit,a.ctx, nr, &iocbs_p)

#define AIOS_SELECT_STRATEGY(a,threshold,worker_ind,ev_get_num)\
do{\
int min=a.cb_array_cap,max=0,min_ind=0;\
for(int i=0;i<workers;i++){\
	ev_get_num=a.cb_array_cap-(a.H_E[DIM_2_INDEX({workers,2},{i,1})]-a.H_E[DIM_2_INDEX({workers,2},{i,0})]+1+a.cb_array_cap)%(a.cb_array_cap+1);\
	min>ev_get_num?(min=ev_get_num)&&min_ind=i:NULL;\
	max<ev_get_num?(max=ev_get_num)&&worker_ind=i:NULL;\
}\
	ev_get_num=max-min;\
	threshold>max?NULL:ev_get_num<threshold?(ev_get_num=threshold):NULL;\
}while(0)

#define AIOS_WAIT(a,threshold,timeout,worker_ind,ev_get_num)\
do{\
	AIOS_SELECT_STRATEGY(a,threshold,worker_ind,ev_get_num)\
	int E=a.H_E[DIM_2_INDEX({workers,2},{worker_ind,1})];\
	int tail=a.cb_array_cap-E+1;\
	if(ev_get_num<1);\
	else if(ev_get_num<=tail){\
		ev_get_num=syscall(SYS_io_getevents,a.ctx, 1, ev_get_num, a.events+DIM_2_INDEX({wokers,a.cb_array_cap+1},{worker_ind,E}), &(timeout));\
	}else{\
		int x=syscall(SYS_io_getevents,a.ctx, 1, tail, a.events+DIM_2_INDEX({wokers,a.cb_array_cap+1},{worker_ind,E}), &(timeout));\
		if(x<tail)\
			ev_get_num=x;\
		else{\
			ev_get_num=syscall(SYS_io_getevents,a.ctx, 1, ev_get_num-tail, a.events+DIM_2_INDEX({wokers,a.cb_array_cap+1},{worker_ind,0}), &(timeout));\
			ev_get_num=ev_get_num<0?tail:(ev_get_num+tail);\
		}\
	}\
	a.H_E[DIM_2_INDEX({workers,2},{worker_ind,1})]=(E+ev_get_num)%(1+a.cb_array_cap);\
}while(0)


#define AIOS_DESTROY(a)\
    free(a.events);free(a.H_E);syscall(SYS_io_destroy,a.ctx)




#endif /* SRC_AIOS_AIOS_H_ */
