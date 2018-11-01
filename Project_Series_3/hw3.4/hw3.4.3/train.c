#define _GNU_SOURCE  // for pthread_yield
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include "myccr.h"
#define T 4

CCR_DECLARE(R1)

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

int passengers_ready_to_exit = 0;
int passengers_ready = 0;
int train_waiting_to_start_ride = 0;
int train_waiting_to_end = 0;
int rest = 0;

// Train entry code
int enter_the_train(train_info *passenger_info){
	long unsigned self = pthread_self();
	int not_getting_on_the_train = 0;
	
	if(passenger_info->nofrides == 0){
		printf("\033[1m\033[33m" "Line:%d Thread %lu  couln't make it into the train\n" "\033[0m",__LINE__,self);
		return 1;
	}
	
	CCR_EXEC(R1, ( ((train_waiting_to_start_ride == 1) && (passenger_info->counter < passenger_info->N)) || (passenger_info->train_done == 1)),
		passenger_info->counter++;
		
		if(passenger_info->train_done == 1){
			not_getting_on_the_train = 1;
			printf("\033[1m\033[33m" "Line:%d Thread %lu  couln't make it into the train\n" "\033[0m",__LINE__,self);
		}
		else if(passenger_info->counter == passenger_info->N) {
			passengers_ready = 1;
			train_waiting_to_start_ride = 0;
			printf("\033[1m\033[32m" "Line:%d Thread %lu entered the train\n" "\033[0m",__LINE__,self);
		}
		else{
			printf("\033[1m\033[32m" "Line:%d Thread %lu entered the train\n" "\033[0m",__LINE__,self);
		}
	)
	
	if(not_getting_on_the_train == 1){
		return 1;
	}
	
	return 0;
}

// Train exit code
void exit_the_train(train_info *passenger_info){
	long unsigned self = pthread_self();
	
	CCR_EXEC(R1, (train_waiting_to_end == 1),
		passenger_info->counter--;
		if(passenger_info->counter == 0) {	// Last passenger entry
			passengers_ready_to_exit = 1;
			train_waiting_to_end = 0;
		}
	)
	printf("\033[1m\033[31m" "Line:%d Thread %lu exited the train\n" "\033[0m",__LINE__,self);
}


void *train_foo(void *arg){
	train_info *ride_info = (train_info *)arg;
	int ride_number = 1;
	
	if(ride_info->nofrides == 0){
		printf("\033[1m\033[36m" "Not enough people for the train to perform a ride\n" "\033[0m");
		return NULL;
	}
	
	while(1){
		printf("train loop starts\n");
		
		CCR_EXEC(R1, 1,
			train_waiting_to_start_ride = 1;
		)
		
		CCR_EXEC(R1, (passengers_ready == 1),
			passengers_ready = 0;
		)
		
		// Start of CS
		sleep(T);
		
		CCR_EXEC(R1, 1,
			train_waiting_to_end = 1;
		)
		
		CCR_EXEC(R1, (passengers_ready_to_exit == 1),
			passengers_ready_to_exit = 0;
			// if there are not enough people to complete another ride turn the train off
			if(ride_info->nofrides == ride_number){
				ride_info->train_done = 1;
			}
		)
		
		sleep(2);
		
		if(ride_info->train_done == 1){
			printf("Train turned off\n");
			return NULL;
		}
		
		
		
		ride_number++;
		
		printf("train loop ends\n");
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
	
	// CS
	
	exit_the_train(passenger_info);
	
	return NULL;
}

int main(int argc,char *argv[]){
	char *input_filename, space, buffer[1];
	int input_fd, res, res_read, interval = 0, i = 0, nofthreads;
	pthread_t *passenger_thread,train_thread;
	train_info main_info;
	off_t fsize;
	
	CCR_INIT(R1)
	
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
	
	// find the number of rides that will be completed in order to terminate the program
	fsize = lseek(input_fd, 0, SEEK_END);
	main_info.nofrides = ((fsize+1) / (2*main_info.N));
	rest =  ((fsize+1) % (2*main_info.N));
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
	return 0;
}
