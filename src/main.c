/*
 ============================================================================
 Name        : x_event_driver.c
 Author      : xpc
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <sys/eventfd.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include<assert.h>
/**
 * GCC support blocks method
 *	#define cgs_lambda( return_type, function_body) \
 *		   ({return_type  function_body ;})
 *
 *	#define $cgs_lambda
 */
/**
 * clang support blocks method ,but need libblocksruntime (-fblocks -lBlocksRuntime)
 * void (^hello)(void)=^(void){printf("Hello, block!\n");}();
 */

#define handle_error(msg) \
           do { perror(msg); exit(EXIT_FAILURE); } while (0)

int main(void) {
	int efd, j;
	uint64_t u=0x8fffffffffffffff;
	ssize_t s;

	efd = eventfd(0, 0);
	if (efd == -1)
		handle_error("eventfd");

    switch (fork()) {
    case 0:
    	printf("%llu \n",(unsigned long long)u);
        for (j = 1; j < 2; j++) {
            printf("%d\n",eventfd_write(efd, u));
        }
        printf("Child completed write loop\n");

        exit(EXIT_SUCCESS);

    default:
    	sleep(5);

        printf("Parent about to read\n");
        printf("%d\n",eventfd_read(efd, &u));
        printf("Parent read %llu (0x%llx) from efd\n",(unsigned long long) u, (unsigned long long) u);
        exit(EXIT_SUCCESS);

    case -1:
        handle_error("fork");
    }
}

int add(const int a, const int b) {
	static_assert(sizeof('a') == 4, "");
	return a + b;
}
