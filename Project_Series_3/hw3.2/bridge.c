#define _GNU_SOURCE  // for pthread_yield
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#define T 5

volatile int blue_on_bridge = 0;
volatile int red_on_bridge = 0;
volatile int blue_waiting = 0;
volatile int red_waiting = 0;
volatile int total_counter = 0;
volatile int N;					// Max number of cars on the bridge

pthread_cond_t rq;
pthread_cond_t bq;
pthread_mutex_t mtx;

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
	long unsigned self = pthread_self();
	if(colour =='r'){
		pthread_mutex_lock(&mtx);
		if(  (blue_on_bridge > 0) || (red_on_bridge > (N-1)) || ( (blue_waiting > 0) && (total_counter > ((2*N) - 1)) )  ) {
			red_waiting++;
			printf("red_waiting got increased from enter to: %d\n", red_waiting);
			pthread_cond_wait(&rq, &mtx);
			
			if (red_waiting > 0 && total_counter < ((2*N) - 1) && (red_on_bridge < N)) {
				red_waiting--;
				red_on_bridge++;
				total_counter++;
				printf("red_waiting got decreased from enter to: %d\n", red_waiting);
				printf("Red on bridge: %d, Total counter: %d got increased by thread %lu\n", red_on_bridge, total_counter, self);
				pthread_cond_signal(&rq);
			}
		}
		else{
			red_on_bridge++;
			total_counter++;
			printf("Red on bridge: %d, Total counter: %d got increased by thread %lu\n", red_on_bridge, total_counter, self);
		}
		
		pthread_mutex_unlock(&mtx);
	}
	else{
		pthread_mutex_lock(&mtx);
		if (   (red_on_bridge > 0) || (blue_on_bridge > (N-1)) || ( (red_waiting > 0) && (total_counter > ((2*N) - 1)) )   ) {
			blue_waiting++;
			printf("blue_waiting got increased from enter to: %d\n", blue_waiting);
			pthread_cond_wait(&bq, &mtx);
			if (blue_waiting > 0 && total_counter < ((2*N) - 1) && (blue_on_bridge < N)) {
				blue_waiting--;
				blue_on_bridge++;
				total_counter++;
				printf("blue_waiting got decreased from enter to: %d\n", blue_waiting);
				printf("Blue on bridge: %d, Total counter: %d got increased by thread %lu\n", blue_on_bridge, total_counter, self);
				pthread_cond_signal(&bq);
			}
		}
		else{
			blue_on_bridge++;
			total_counter++;
			printf("Blue on bridge: %d, Total counter: %d got increased by thread %lu\n", blue_on_bridge, total_counter, self);
		}
		pthread_mutex_unlock(&mtx);
	}
}

void exit_the_bridge(char colour){
	long unsigned self = pthread_self();
	if(colour =='r'){
		pthread_mutex_lock(&mtx);
		red_on_bridge--;
		printf("Red on bridge: %d got decreased by thread %lu\n", red_on_bridge, self);
		if(  (red_waiting > 0) && (red_on_bridge < N) && ((total_counter < (2*N))||(blue_waiting == 0))  ) {
			red_waiting--;
			red_on_bridge++;
			total_counter++;
			printf("red_waiting got decreased from exit to: %d\n", red_waiting);
			printf("Red on bridge: %d, Total counter: %d got increased by thread %lu\n", red_on_bridge, total_counter, self);
			pthread_cond_signal(&rq);
		}
		else if ((red_on_bridge == 0) && (blue_waiting > 0)) {
			blue_waiting--;
			total_counter = 0;
			blue_on_bridge++;
			total_counter++;
			printf("blue_waiting got decreased from exit to: %d\n", blue_waiting);
			printf("Blue on bridge: %d, Total counter: %d got increased by thread %lu\n", blue_on_bridge, total_counter, self);
			pthread_cond_signal(&bq);
		}
		pthread_mutex_unlock(&mtx);
	}
	else{
		pthread_mutex_lock(&mtx);
		blue_on_bridge--;
		printf("Blue on bridge: %d got decreased by thread %lu\n", blue_on_bridge, self);
		if(  ((blue_waiting > 0)&&(blue_on_bridge < N))  &&  ((total_counter < (2*N))||(red_waiting == 0))  ) {
			blue_waiting--;
			blue_on_bridge++;
			total_counter++;
			printf("blue_waiting got decreased from exit to: %d\n", blue_waiting);
			printf("Blue on bridge: %d, Total counter: %d got increased by thread %lu\n", blue_on_bridge, total_counter, self);
			pthread_cond_signal(&bq);
		}
		else if ((blue_on_bridge == 0) && (red_waiting > 0)) {
			red_waiting--;
			total_counter = 0;
			red_on_bridge++;
			total_counter++;
			printf("red_waiting got decreased from exit to: %d\n", red_waiting);
			printf("Red on bridge: %d, Total counter: %d got increased by thread %lu\n", red_on_bridge, total_counter, self);
			pthread_cond_signal(&rq);
		}
		pthread_mutex_unlock(&mtx);
	}
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
	int  mtxtype = PTHREAD_MUTEX_NORMAL;
	pthread_mutexattr_t attr;
	
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
	
	car_thread = (pthread_t *)malloc(0);
	colour = (char *)malloc(0);
	
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, mtxtype);
	
	pthread_mutex_init(&mtx, &attr);
	pthread_cond_init(&rq, NULL);
	pthread_cond_init(&bq, NULL);
	
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
	for(i=0;i<nofthreads;i++){
		res = pthread_join(car_thread[i], NULL);
		if(res){
			printf("Error with thread join number %d\n", i);
			exit(1);
		}
	}
	
	
	
	sleep(2);	// for printf completion
	printf("\n---->All threads joined<----\n");
	
	return 0;
}
