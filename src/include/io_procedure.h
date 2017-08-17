/*
 * io_operation.h
 *
 *  Created on: Jan 6, 2017
 *      Author: xpc
 */

#ifndef INCLUDE_IO_PROCEDURE_H_
#define INCLUDE_IO_PROCEDURE_H_

typedef struct EVENT_PROC{
	void* data;
	void(*callback)(void*);
}EventProc;

typedef struct EVENT_WORKER{
	aio_context_t ctx;
	void* queue;
	int* head;
	int* end;
	int* left;
	int singleCap;
	int workers;
	int* indexArray;//for sort
	int* efd;//threads comunication
}EventWorker;

int init();
int submit();
int ioCompleteEventListen();
int onIOComplete(EventWorker* ew,unsigned short workerId);
int destroy();


#endif /* INCLUDE_IO_PROCEDURE_H_ */
