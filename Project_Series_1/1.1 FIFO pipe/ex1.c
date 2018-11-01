//FIFO pipe
#define _GNU_SOURCE  // for pthread_yield
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>


volatile char *buffer;
volatile int in = 0, out = 0, threads_finished = 0, is_pipe_closed = 0;

typedef struct parameters{
	char *input_filename, *output_filename;
	int pipe_size, input_fd, output_fd;
}buf_info;


//Initialization of the buffer
void pipe_init(int size){
	int i=0;
	
	buffer = (char *)malloc(size);
	
	if(buffer == NULL){
		printf("Problem with malloc at line %d\n", __LINE__);
		exit(1);
	}
	for(i=0; i<size; i++){
		buffer[i] = '\0';
	}
}

//Read one byte from the pipe
int pipe_read(char *c, buf_info *pipe_read_info){
	
	// if the buffer is empty wait for a slot to be written on
	while((out%(pipe_read_info->pipe_size)) == (in%(pipe_read_info->pipe_size))) {
		if(is_pipe_closed == 1)
			return 0;
		pthread_yield();
	}
	
	*c = buffer[out];
	out = (out+1)%(pipe_read_info->pipe_size);
	
	return 1;
}

//Write one byte to the pipe
void pipe_write(char c, buf_info *pipe_write_info){
	
	//if the buffer is full wait for a slot to open
	while( ((in+1) % (pipe_write_info->pipe_size)) == (out % (pipe_write_info->pipe_size))){
		pthread_yield();
	}
	
	buffer[in] = c;
	in = (in+1)%(pipe_write_info->pipe_size);
}


void pipe_close(){
	
	is_pipe_closed = 1;
	
}
  
// Function that reads data from input file and writes it to the pipe
void *write_foo(void *arg){
	int res_read;
	char c;
	buf_info *write_info = (buf_info *)arg;
	
	write_info->input_fd = open(write_info->input_filename, O_RDONLY, S_IRWXU);
	if(write_info->input_fd == -1){
		printf("Open at line %d failed\n", __LINE__);
		exit(1);
	}
	
	while(1){
		res_read = read(write_info->input_fd, &c, sizeof(char));
		if(res_read == 0){
			// EOF reached
			break;
		}
		else if(res_read == -1){
			printf("Read at line %d failed\n", __LINE__);
			exit(1);
		}
		pipe_write(c, write_info);
	}
	
	pipe_close();
	
	threads_finished += 1;
	
	return NULL;
}

// Function that reads data from the pipe and writes it to output file
void *read_foo(void *arg){
	int res_write, res_pipe_read;
	char c;
	buf_info *read_info = (buf_info *)arg;
	
	read_info->output_fd = open(read_info->output_filename, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU);
	if(read_info->output_fd == -1){
		printf("Open at line %d failed\n", __LINE__);
		exit(1);
	}
	
	while(1){
		res_pipe_read = pipe_read(&c, read_info);
		if (res_pipe_read == 0)
			break;
		res_write = write(read_info->output_fd, &c, sizeof(char));
		if(res_write == -1){
			printf("Write at line %d failed\n", __LINE__);
			exit(1);
		}
	}
	
	threads_finished += 1;
	
	return NULL;
}



int main(int argc,char *argv[]){
	pthread_t read_thread, write_thread;     // read_thread,write_thread: IDs of the threads
	int res_read, res_write;                 // variables to check the return value of pthread_create
	buf_info main_info;                      // struct variable
	
	//Check the number of arguments - Initialize the fields(pipe_size, input_filename, output_filename) of the struct
	if(argc != 4){
		printf("Please provide:\n -> The buffer size as the first argument\n -> The input file name as the second argument\n -> The output file name as the third argument\n");
		return 1;
	}
	
	main_info.pipe_size = atoi(argv[1]);
	main_info.input_filename = argv[2];
	main_info.output_filename = argv[3];
	
	if(main_info.pipe_size < 2){
		printf("Please enter a number for the pipe size higher or equal to 2\n");
		return 1;
	}
	
	pipe_init(main_info.pipe_size);
	
	//Create the write thread
	res_write = pthread_create(&write_thread, NULL, write_foo, (void *)&main_info);
	if(res_write){
		printf("Error with write thread creation\n");
		exit(1);
	}
	
	//Create the read thread
	res_read = pthread_create(&read_thread, NULL, read_foo, (void *)&main_info);
	if(res_read){
		printf("Error with read thread creation\n");
		exit(1);
	}
	
	//Check if the threads have finished
	while(threads_finished < 2){
		pthread_yield();
	}
	
	close(main_info.input_fd);
	close(main_info.output_fd);
	
	free((void *)buffer);
	
	return 0;
}

