#define _GNU_SOURCE  // for pthread_yield
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include "myccr.h"
#define T 5

volatile int blue_on_bridge = 0;
volatile int red_on_bridge = 0;
volatile int blue_waiting = 0;
volatile int red_waiting = 0;
volatile int blue_counter = 0;
volatile int red_counter = 0;
volatile int N;					// Max number of cars on the bridge

CCR_DECLARE(R1)

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
// 	long unsigned self = pthread_self();
	if(colour =='r'){ 
		CCR_EXEC(R1,1,
			red_waiting++;
		)
		
		CCR_EXEC(R1, (red_on_bridge < N) && ( (red_counter < (2*N)) || (blue_waiting == 0) ) && (blue_on_bridge == 0) && !( (blue_counter < (2*N)) && (blue_counter > 0) && (blue_waiting > 0) ),
			blue_counter = 0;
			red_on_bridge++;
			red_counter++;
			red_waiting--;
		)
	}
	else{
		CCR_EXEC(R1,1,
			blue_waiting++;
		)
		
		CCR_EXEC(R1, (blue_on_bridge < N) && ( (blue_counter < (2*N)) || (red_waiting == 0) ) && (red_on_bridge == 0) && !( (red_counter < (2*N)) && (red_counter > 0) && (red_waiting > 0) ),
			red_counter = 0;
			blue_on_bridge++;
			blue_counter++;
			blue_waiting--;
		)
	}
}

void exit_the_bridge(char colour){
// 	long unsigned self = pthread_self();
	if(colour =='r'){
		CCR_EXEC(R1, 1,
			red_on_bridge--;
		)
	}
	else{
		CCR_EXEC(R1, 1,
			blue_on_bridge--;
		)
		
		
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
	
	CCR_INIT(R1)
	
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
