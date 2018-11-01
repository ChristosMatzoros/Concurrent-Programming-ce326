#define _GNU_SOURCE  // for pthread_yield
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include "mybsem.h"
#define T 5

mybsem sem_order;				// Prevent starvation 
mybsem sem_bridge;				// Determine what colour of cars can be on the bridge
mybsem wait_car;				// Wait for the bridge to get full
mybsem sem[2];					// Mutual exclusion for the counter of each car colour
volatile int car_count[2];		// Number of cars for every colour
volatile int N;					// Max number of cars on the bridge

int read_error_check(int res){
	if(res == 0){
		// EOF reached
		return 1;
	}
	else if(res == -1){
		printf("Read at line %d failed\n", __LINE__);
		exit(1);
	}
	return 0;
}

void enter_the_bridge(char colour){
	int c;
	
	if(colour =='r'){ 
		c = 0; 
	}
	else{ 
		c = 1;
	}
	
	mybsem_down(&sem_order);
	mybsem_down(&sem[c]);
	
	car_count[c]++ ;
	
	// First car to enter the bridge
	if(car_count[c] == 1){
		mybsem_down(&sem_bridge);
	}
	
	if(car_count[c] > N){
		mybsem_up(&sem[c]);
		mybsem_down(&wait_car);
	}
	else{
		mybsem_up(&sem[c]);
	}
	mybsem_up(&sem_order);
}

void exit_the_bridge(char colour){
	int c;
	
	if(colour =='r'){ 
		c = 0; 
	}
	else{ 
		c = 1;
	}

	mybsem_down(&sem[c]);

	car_count[c]--;
	
	if(car_count[c] == N){
		mybsem_up(&wait_car);
	}
	
	if(car_count[c] == 0){
		mybsem_up(&sem_bridge);
	}
	
	mybsem_up(&sem[c]);
}

void *foo(void *arg){
	char colour = *(char *)arg; 
	long unsigned self = pthread_self();

	enter_the_bridge(colour);

	if(colour =='r'){ 
		printf("\033[1m\033[31m" "Line:%d Thread %lu red car in CS\n" "\033[0m",__LINE__,self);
	}
	else{ 
		printf("\033[1m\033[34m" "Line:%d Thread %lu blue car in CS\n" "\033[0m",__LINE__,self);
	}

	sleep(T);

	if(colour =='r'){ 
		printf("\033[1m\033[31m" "Line:%d Thread %lu red car exited CS\n" "\033[0m",__LINE__,self);
	}
	else{ 
		printf("\033[1m\033[34m" "Line:%d Thread %lu blue car exited CS\n" "\033[0m",__LINE__,self);
	}
	
	
	exit_the_bridge(colour);
	
	return NULL;
}
	
int main(int argc,char *argv[]){
	char *input_filename, *colour, space, buffer[1];
	int input_fd, res, res_read, interval = 0, i = 0, nofthreads;
	pthread_t *car_thread;
	
	if(argc != 3){
		printf("Please provide:\n -> The number of the cars that fit into the bridge as the first argument\n -> The input file name as the second argument\n");
		return 1;
	}
	
	N = atoi(argv[1]);
	input_filename = argv[2];
	
	// open the input file
	input_fd = open(input_filename, O_RDONLY, S_IRWXU);
	if(input_fd == -1){
		printf("Open at line %d failed\n", __LINE__);
		exit(1);
	}
	
	// Initialize the semaphores
	mybsem_init(&sem[0],1);
	mybsem_init(&sem[1],1);
	mybsem_init(&sem_order,1);
	mybsem_init(&sem_bridge,1);
	mybsem_init(&wait_car,0);
	
	car_count[0] = 0;
	car_count[1] = 0;
	
	car_thread = (pthread_t *)malloc(0);
	colour = (char *)malloc(0);

	while(1) {
		car_thread = (pthread_t *) realloc(car_thread, sizeof(pthread_t) * (i+1));
		colour = (char *) realloc(colour, sizeof(char) * (i+1));

		// read the car colour
		res_read = read(input_fd, &colour[i], sizeof(char));
		if(read_error_check(res_read) == 1){
			// EOF reached
			break;
		}
		
		if( (colour[i]!='r') && (colour[i]!='b') ){
			printf("Invalid colour entered\n");
			return 1;
		}
		
		// read the space
		res_read = read(input_fd, &space, sizeof(char));
		if(read_error_check(res_read) == 1){
			// EOF reached
			break;
		}
		
		// read the interval to spawn the next car
		res_read = read(input_fd, &buffer, sizeof(char));
		if(read_error_check(res_read) == 1){
			// EOF reached
			break;
		}
		interval = atoi(buffer);
		
		printf("Read the thread info: Colour %c, Interval %d\n", colour[i], interval);
		
		// wait before creating the thread
		sleep(interval);
		
		// create the thread
		res = pthread_create(&car_thread[i], NULL, foo, (void *)&colour[i]);
		if(res){
			printf("Error with the car thread creation\n");
			exit(1);
		}
		// read the space
		res_read = read(input_fd, &space, sizeof(char));
		if(read_error_check(res_read) == 1){
			// EOF reached
			break;
		}
		i++;
	}
	
	nofthreads = i;
	printf("nofthreads %d\n", nofthreads);
	// Wait for all the car threads to terminate
	for(i=0;i<nofthreads+1;i++){
		res = pthread_join(car_thread[i], NULL);
		if(res){
			printf("Error with thread join number %d\n", i);
			exit(1);
		}
	}
	
	printf("\n---->All threads joined<----\n");
	
	// Destroy the semaphores
	mybsem_destroy(&sem[0]);
	printf("sem[0] destroyed\n");
	mybsem_destroy(&sem[1]);
	printf("sem[1] destroyed\n");
	mybsem_destroy(&sem_order);
	printf("sem_order destroyed\n");
	mybsem_destroy(&sem_bridge);
	printf("sem_bridge destroyed\n");
	mybsem_destroy(&wait_car);
	printf("wait_car destroyed\n");
	
	printf("\n---->All semaphores destroyed<----\n");
	
	return 0;
}