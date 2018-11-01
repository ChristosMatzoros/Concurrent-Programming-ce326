#define _GNU_SOURCE  // for pthread_yield
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include "mybsem.h"

void *foo1(void *arg){
	long unsigned self = pthread_self();
	mybsem *sem = (mybsem *)arg;
	mybsem_down(sem);
	
	printf("Thread foo1 with id %lu arrived\n", self);
	
	return NULL;
}

void *foo2(void *arg){
	long unsigned self = pthread_self();
	mybsem *sem = (mybsem *)arg;

	printf("Thread foo2 with id %lu arrived\n", self);
	
	mybsem_up(sem);
	
	return NULL;
}


int main(int argc,char *argv[]){
	pthread_t thread1,thread2;	//IDs of the threads
	int res1, res2;				// variables to check the return value of pthread_create
	mybsem sem;
	
	mybsem_init(&sem, 0);
	
	res1= pthread_create(&thread1, NULL, foo1, (void *)&sem);
	if(res1){
		printf("Error with 1 thread creation\n");
		exit(1);
	}
	
	res2 = pthread_create(&thread2, NULL, foo2, (void *)&sem);
	if(res2){
		printf("Error with 2 thread creation\n");
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
	
	printf("Both threads joined\n");
	
	mybsem_destroy(&sem);
	
	return 0;
}


