#define _GNU_SOURCE  // for pthread_yield
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#define T 4

pthread_mutex_t mtx;
pthread_cond_t train_cond, passengers_cond1, passengers_cond2;

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
	
	pthread_mutex_lock(&mtx);
	
	if(passenger_info->nofrides == 0){
		printf("\033[1m\033[33m" "Line:%d Thread %lu  couln't make it into the train\n" "\033[0m",__LINE__,self);
		pthread_mutex_unlock(&mtx);
		return 1;
	}
	
	passenger_info->counter++;
	
	if( (passenger_info->counter % passenger_info->N) == 0) {	// Last passenger entry
		passengers_ready++;
		if(train_waiting_to_start_ride == 1){
			pthread_cond_signal(&train_cond); 
		}
	}
	printf("\033[1m\033[35m" "Line:%d Thread %lu waiting\n" "\033[0m",__LINE__,self);
	pthread_cond_wait(&passengers_cond1, &mtx);
	if(passenger_info->train_done == 1){
		printf("\033[1m\033[33m" "Line:%d Thread %lu  couln't make it into the train\n" "\033[0m",__LINE__,self);
		pthread_mutex_unlock(&mtx);
		return 1;
	}
	printf("\033[1m\033[32m" "Line:%d Thread %lu entered the train\n" "\033[0m",__LINE__,self);
	pthread_mutex_unlock(&mtx);

	return 0;
}

// Train exit code
void exit_the_train(train_info *passenger_info){
	long unsigned self = pthread_self();
	pthread_mutex_lock(&mtx);
	passengers_ready_to_exit++;

	if(train_waiting_to_end == 1 && passengers_ready_to_exit == passenger_info->N){
		pthread_cond_signal(&train_cond);
	}
	pthread_cond_wait(&passengers_cond2, &mtx);
	printf("\033[1m\033[31m" "Line:%d Thread %lu exited the train\n" "\033[0m",__LINE__,self);

	pthread_mutex_unlock(&mtx);
}

void *train_foo(void *arg){
	train_info *ride_info = (train_info *)arg;
	int ride_number = 1, i;
	
	if(ride_info->nofrides == 0){
		printf("\033[1m\033[36m" "Not enough people for the train to perform a ride\n" "\033[0m");
		return NULL;
	}

	while(1){
		pthread_mutex_lock(&mtx);
		printf("train loop starts\n");
		if(passengers_ready == 0){
			train_waiting_to_start_ride = 1;
			pthread_cond_wait(&train_cond, &mtx); 
		}
		train_waiting_to_start_ride = 0;
		passengers_ready--;
		
		for(i=0; i<ride_info->N; i++){
			pthread_cond_signal(&passengers_cond1);
		}
		
		pthread_mutex_unlock(&mtx);
		
		// Start of CS
		sleep(T);
		
		pthread_mutex_lock(&mtx);
		
		if(passengers_ready_to_exit < ride_info->N){
			train_waiting_to_end = 1;
			pthread_cond_wait(&train_cond, &mtx); 
		}
		passengers_ready_to_exit = 0;
		train_waiting_to_end = 0;
		for(i=0; i<ride_info->N; i++){
			pthread_cond_signal(&passengers_cond2);
		}
		
		pthread_mutex_unlock(&mtx);
		
		sleep(2);
		
		// if there are not enough people to complete another ride turn the train off
		if(ride_info->nofrides == ride_number){
			ride_info->train_done = 1;
			printf("Train turned off\n");
			for(i=0; i<rest; i++){
				pthread_cond_signal(&passengers_cond1);
			}
			break;
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
	int  mtxtype = PTHREAD_MUTEX_NORMAL;
	pthread_mutexattr_t attr;
	
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
	
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, mtxtype);
	
	pthread_mutex_init(&mtx, &attr);
	pthread_cond_init(&train_cond, NULL);
	pthread_cond_init(&passengers_cond1, NULL);
	pthread_cond_init(&passengers_cond2, NULL);
	
	
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
