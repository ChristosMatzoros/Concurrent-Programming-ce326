#define _GNU_SOURCE  // for pthread_yield
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include "mybsem.h"
#define T 4

mybsem counter_sem;			// Mutual exclusion for counter
mybsem ready_to_start;		// Wait for the last passenger to get into the train in order to start the ride
mybsem waiting_room;		// Wait for the train to arrive
mybsem wait_passengers;		// Wait for the train to get full
mybsem train_loaded;		// Wait for the train to start the ride
mybsem ready_to_disembark;	// Wait for the train to reach the destination in order to disembark
mybsem leave;				// Wait for every passenger to get out
mybsem next_ride;			// Wait for the train to empty


typedef struct information{
	int N;
	int counter;
	int nofrides;
	int train_done;
}train_info;

// Check the return value from function read
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

// Train entry code
int enter_the_train(train_info *passenger_info){
	long unsigned self = pthread_self();
	
	if(passenger_info->nofrides == 0){
		printf("\033[1m\033[33m" "Line:%d Thread %lu  couln't make it into the train\n" "\033[0m",__LINE__,self);
		return 1;
	}

	mybsem_down(&waiting_room); 
	
	if(passenger_info->train_done == 1){
		printf("\033[1m\033[33m" "Line:%d Thread %lu  couln't make it into the train\n" "\033[0m",__LINE__,self);
		mybsem_up(&waiting_room);
		return 1;
	}
	
	mybsem_down(&counter_sem);
	
	passenger_info->counter++;
	if(passenger_info->counter == 1){	// First passenger entry
		printf("\n");
		printf("\033[1m\033[32m" "Line:%d Thread %lu entered the train\n" "\033[0m",__LINE__,self);
		mybsem_down(&wait_passengers);	// wait in the queue
		mybsem_up(&counter_sem);
		mybsem_up(&waiting_room);
		mybsem_down(&train_loaded);		// first passenger get into the train and wait the Nth passenger 
		mybsem_up(&wait_passengers);
	}
	else if(passenger_info->counter < passenger_info->N){
		printf("\033[1m\033[32m" "Line:%d Thread %lu entered the train\n" "\033[0m",__LINE__,self);
		mybsem_up(&counter_sem);
		mybsem_up(&waiting_room);
		mybsem_down(&wait_passengers);	// wait in the queue
		mybsem_up(&wait_passengers);
	}
	else if(passenger_info->counter == passenger_info->N){	// Last passenger entry
		printf("\033[1m\033[32m" "Line:%d Thread %lu entered the train\n" "\033[0m",__LINE__,self);
		mybsem_up(&counter_sem);
		printf("\n All passengers entered the train \n\n");
		mybsem_up(&ready_to_start);
	}
	return 0;
}

// Train exit code
void exit_the_train(train_info *passenger_info){
	long unsigned self = pthread_self();
	
	mybsem_down(&counter_sem);
	passenger_info->counter--;
	
	mybsem_down(&leave);
	
	if(passenger_info->counter == 0){
		printf("\033[1m\033[31m" "Line:%d Thread %lu  exited the train\n" "\033[0m",__LINE__,self);
		mybsem_up(&counter_sem);
		mybsem_up(&next_ride);
	}
	else if(passenger_info->counter == (passenger_info->N)-1){
		mybsem_up(&counter_sem);
		mybsem_down(&ready_to_disembark);
		printf("\033[1m\033[31m" "Line:%d Thread %lu  exited the train\n" "\033[0m",__LINE__,self);
	}
	else{
		printf("\033[1m\033[31m" "Line:%d Thread %lu  exited the train\n" "\033[0m",__LINE__,self);
		mybsem_up(&counter_sem);
	}
	mybsem_up(&leave);
}


void *train_foo(void *arg){
	train_info *ride_info = (train_info *)arg;
	int ride_number = 1;
	
	if(ride_info->nofrides == 0){
		printf("\033[1m\033[36m" "Not enough people for the train to perform a ride\n" "\033[0m");
		return NULL;
	}

	while(1){
		mybsem_down(&ready_to_start);
		mybsem_up(&train_loaded);
		
		// Start of CS
		printf("\033[1m\033[35m" "Ride number %d Begins\n" "\033[0m", ride_number);
		sleep(T);
		printf("\033[1m\033[36m" "Ride number %d Ends\n" "\033[0m", ride_number);
		
		mybsem_up(&ready_to_disembark);
		sleep(2);
		
		mybsem_down(&next_ride);
		
		// if there are not enough people to complete another ride turn the train off
		if(ride_info->nofrides == ride_number){
			ride_info->train_done = 1;
			mybsem_up(&waiting_room);
			break;
		}
	
		mybsem_up(&waiting_room);
		ride_number++;
	}
	
	return NULL;
}

void *passenger_foo(void *arg){
	int res = 0;
	train_info *passenger_info = (train_info *)arg;
	
	res = enter_the_train(passenger_info);
	if (res == 1){
		return NULL;
	}
	
	exit_the_train(passenger_info);
	
	return NULL;
}

int main(int argc,char *argv[]){
	char *input_filename, space, buffer[1];
	int input_fd, res, res_read, interval = 0, i = 0, nofthreads;
	pthread_t *passenger_thread,train_thread;
	train_info main_info;
	off_t fsize;

	if(argc != 3){
		printf("Please provide:\n -> The number of the train seats as the first argument\n -> The input file name as the second argument\n");
		return 1;
	}
	
	main_info.N = atoi(argv[1]);
	input_filename = argv[2];
	
	if(main_info.N < 2){
		printf("The train must have at least two seats!\n");
		return 1;
	}
	
	// open the input file
	input_fd = open(input_filename, O_RDONLY, S_IRWXU);
	if(input_fd == -1){
		printf("Open at line %d failed\n", __LINE__);
		exit(1);
	}
	
	
	main_info.counter = 0;
	main_info.nofrides = 0;
	main_info.train_done = 0;
	
	mybsem_init(&next_ride, 0);
	mybsem_init(&train_loaded, 0);
	mybsem_init(&ready_to_start, 0);
	mybsem_init(&ready_to_disembark, 0);
	
	mybsem_init(&wait_passengers, 1);
	mybsem_init(&counter_sem, 1);
	mybsem_init(&waiting_room, 1);
	mybsem_init(&leave, 1);

	// find the number of rides that will be completed in order to terminate the program
	fsize = lseek(input_fd, 0, SEEK_END);
	main_info.nofrides = ((fsize+1) / (2*main_info.N));

	fsize = lseek(input_fd, 0, SEEK_SET);

	res = pthread_create(&train_thread, NULL, train_foo, (void *)&main_info);
	if(res){
		printf("Error with the train thread creation\n");
		exit(1);
	}
	
	passenger_thread = (pthread_t *)malloc(0);
	
	while(1) {
		passenger_thread = (pthread_t *) realloc(passenger_thread, sizeof(pthread_t) * (i+1));
		
		// read the interval to spawn the next passenger
		res_read = read(input_fd, &buffer, sizeof(char));
		if(read_error_check(res_read)){
			// EOF reached
			break;
		}
		interval = atoi(buffer);
		
		// wait before creating the thread
		sleep(interval);
		
		// create the thread
		res = pthread_create(&passenger_thread[i], NULL, passenger_foo, (void *)&main_info);
		if(res){
			printf("Error with the passenger thread creation\n");
			exit(1);
		}

		// read the space
		res_read = read(input_fd, &space, sizeof(char));
		if(read_error_check(res_read)){
			// EOF reached
			break;
		}
		i++;
	}
	
	nofthreads = i;
	
	// Wait for the passenger threads to join
	for(i=0;i<nofthreads;i++){
		res = pthread_join(passenger_thread[i], NULL);
		if(res){
			printf("Error with thread join number %d\n", i);
			exit(1);
		}
	}
	
	// Wait for the train thread to join
	res = pthread_join(train_thread, NULL);
	if(res){
		printf("Error with thread join number %d\n", i);
		exit(1);
	}

	printf("\n---->All threads joined<----\n");

	// Destroy threads
	mybsem_destroy(&next_ride);
 	mybsem_destroy(&train_loaded);
 	mybsem_destroy(&ready_to_start);
 	mybsem_destroy(&ready_to_disembark);
 	mybsem_destroy(&wait_passengers);
 	mybsem_destroy(&counter_sem);
 	mybsem_destroy(&waiting_room);
 	mybsem_destroy(&leave);
	
	printf("\n---->All semaphores destroyed<----\n");

	return 0;
}