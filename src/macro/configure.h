/*
 * macro_configure.h
 *
 *  Created on: Dec 27, 2016
 *      Author: xpc
 */

#ifndef MACRO_CONFIGURE_H_
#define MACRO_CONFIGURE_H_


#ifdef __DEBUG__
#define DEBUG(format,...) printf("File: "__FILE__", Line: %05d: "format"\n", __LINE__, ##__VA_ARGS__)
#else
#define DEBUG(format,...)
#endif


#define N_CONTAIN 100
#define M_QUEUES 3

#endif /* MACRO_CONFIGURE_H_ */
