#define _GNU_SOURCE  // for pthread_yield
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include "mybsem.h"
#define N 2

void *foo(void *arg){
	long unsigned self = pthread_self();
	mybsem *sem = (mybsem *)arg;
	int i;
	
	for(i=0;i<N;i++){
		mybsem_down(sem);
		
		printf("Thread %lu entered the CS\n", self);
		
		sleep(1);
		printf("...\n");
		
		printf("Thread %lu exited the CS\n", self);
		
		mybsem_up(sem);
	}
	return NULL;
}
	
int main(int argc,char *argv[]){
	pthread_t thread1,thread2,thread3;	//IDs of the threads
	int res1, res2, res3;				// variables to check the return value of pthread_create
	mybsem sem;
	
	mybsem_init(&sem, 1);
	
	res1= pthread_create(&thread1, NULL, foo, (void *)&sem);
	if(res1){
		printf("Error with 1 thread creation\n");
		exit(1);
	}
	
	res2 = pthread_create(&thread2, NULL, foo, (void *)&sem);
	if(res2){
		printf("Error with 2 thread creation\n");
		exit(1);
	}
	
	res3 = pthread_create(&thread3, NULL, foo, (void *)&sem);
	if(res3){
		printf("Error with 3 thread creation\n");
		exit(1);
	}
	
	
	res1 = pthread_join(thread1,NULL);
	if(res1){
		printf("Error with 1st thread join\n");
		exit(1);
	}
	res2 = pthread_join(thread2,NULL);
	if(res2){
		printf("Error with 2nd thread join\n");
		exit(1);
	}
	res3 = pthread_join(thread3,NULL);
	if(res3){
		printf("Error with 3rd thread join\n");
		exit(1);
	}
	
	printf("All 3 threads joined\n");
	
	mybsem_destroy(&sem);
	
	return 0;
}


