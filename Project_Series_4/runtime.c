#define _GNU_SOURCE  // for pthread_yield
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/time.h>
#define BUFFERSIZE 30
#define LINESIZE 100
#define MAX_LINES 3
#define NAMELEN 20
#define create_new_variable(Var, Var_size, name, position)					\
	position = 0;															\
	Var = (table_info *) realloc(Var, sizeof(table_info) * (Var_size+1) );	\
	if(Var == NULL){														\
		printf("ERROR with realloc at line %d\n", __LINE__);				\
	}																		\
	for(position=0;position<NAMELEN;position++){							\
		Var[Var_size].var_name[position] = '\0';							\
	}																		\
	strcpy(Var[Var_size].var_name, name);									\
	position = Var_size;													\
	Var_size++;

#define search_array(name, name_to_search, pos, pos_string, return_pos, Var, Var_size, i, j, name_with_bracket, token, tok)								\
	strcpy(name, tok);																																	\
	tok = strtok(NULL, "[]");																															\
	if(tok[0] == '$'){																																	\
		return_pos =  search_var(Var,&Var_size, tok, 0);																								\
		if(return_pos == -1){																															\
			create_new_variable(Var, Var_size, tok, return_pos)																							\
		}																																				\
		pos = Var[return_pos].value;																													\
	}																																					\
	else{																																				\
		pos = atoi(tok);																																\
	}																																					\
																																						\
	strcpy(name_to_search,name);																														\
	sprintf(pos_string, "%d", pos);																														\
																																						\
	strcat(name_to_search,"[");																															\
	strcpy(name_with_bracket,name_to_search);																											\
	strcat(name_to_search,pos_string);																													\
	strcat(name_to_search,"]");																															\
																																						\
																																						\
	/* Search for the array element */																													\
	return_pos =  search_var(Var,&Var_size, name_to_search, 0);																							\
	if(return_pos == -1){																																\
		for(i = Var_size-1; i >= 0; i--){																												\
			if(strncmp(Var[i].var_name, name_with_bracket, strlen(name_with_bracket)) == 0){															\
				break;																																	\
			}																																			\
		}																																				\
																																						\
		if(i<0){																																		\
			for(j=0; j <= pos; j++){																													\
				strcpy(name_to_search,name);																											\
																																						\
				sprintf(pos_string, "%d", j);																											\
				strcat(name_to_search,"[");																												\
				strcat(name_to_search,pos_string);																										\
				strcat(name_to_search,"]");																												\
																																						\
				create_new_variable(Var, Var_size, name_to_search, return_pos)																			\
			}																																			\
		}																																				\
		else{																																			\
			token = strchr(Var[i].var_name,'[');																										\
			i = atoi(&token[1]);																														\
																																						\
			for(j=i+1; j <= pos; j++){																													\
				strcpy(name_to_search,name);																											\
				sprintf(pos_string, "%d", j);																											\
				strcat(name_to_search,"[");																												\
				strcat(name_to_search,pos_string);																										\
				strcat(name_to_search,"]");																												\
				create_new_variable(Var, Var_size, name_to_search, return_pos)																			\
			}																																			\
		}																																				\
	}


typedef struct information{
	int value;
	char var_name[NAMELEN];
}table_info;

table_info *GlobalVar;
int GlobalVar_size = 0;

volatile int id = 0;
volatile int core_number = 0;
volatile int num_of_cores = 0;

pthread_mutex_t mtx;
pthread_mutex_t mtx2;

int search_var(table_info *var, int *size, char *name, int is_global){
	int i = 0;

	pthread_mutex_lock(&mtx);

	for(i = 0;i < *size; i++){
		if(strcmp(var[i].var_name,name) == 0){
			pthread_mutex_unlock(&mtx);
			return i;
		}
	}
	pthread_mutex_unlock(&mtx);
	return -1;
}



struct list {
	table_info *LocalVar;
	int LocalVar_size;
	FILE *fd;
	int timetostop;
	int is_blocked;
	struct list *next;
	char variable_blocked[LINESIZE];
	char filename[BUFFERSIZE];
	int is_killed;
};

struct list **head;

void init_list(int id) {
	pthread_mutex_lock(&mtx);
	head[id] = (struct list *)malloc(sizeof(struct list));
	head[id]->next = head[id];
	pthread_mutex_unlock(&mtx);
}


int find_core() {
	struct list *current;
	int i = 0;
	int counter = 0;
	int min = 10000;
	int min_id = -1;

	pthread_mutex_lock(&mtx);

	for(i=0;i<num_of_cores;i++){
		for (current = head[i]->next; current != head[i]; current = current->next) {
			counter++;
		}
		if(min>counter){
			min = counter;
			min_id = i;
		}
		counter = 0;
	}
	pthread_mutex_unlock(&mtx);
	return min_id;
}

void print_list() {
	struct list *current;
	int i = 0;
	pthread_mutex_lock(&mtx);
	for(i=0;i<num_of_cores;i++){
		for (current = head[i]->next; current != head[i]; current = current->next) {
			printf("App ID: %d, File name: %s\n", current->LocalVar[0].value, current->filename);
		}
	}
	pthread_mutex_unlock(&mtx);
}

int block(char *sem_name, struct list *node_to_block) {
	struct list *current;
	int i = 0;
	int max_counter = 0;

	pthread_mutex_lock(&mtx);

	for(i=0;i<num_of_cores;i++){
		for (current = head[i]->next; current != head[i]; current = current->next) {
			if((current->is_blocked > 0) && (strcmp(current->variable_blocked, sem_name) == 0)){
				if(max_counter < current->is_blocked){
					max_counter = current->is_blocked;
				}
			}
		}
	}
	node_to_block->is_blocked = max_counter + 1;
	strcpy(node_to_block->variable_blocked, sem_name);
	pthread_mutex_unlock(&mtx);
	return 1;
}

int unblock(char *sem_name) {
	struct list *current;
	int i = 0;
	int counter = 0;

	pthread_mutex_lock(&mtx);
	for(i=0;i<num_of_cores;i++){
		for (current = head[i]->next; current != head[i]; current = current->next) {
			if((current->is_blocked > 0) && (strcmp(current->variable_blocked, sem_name) == 0)){
				current->is_blocked--;
				counter++;
			}
		}
	}
	pthread_mutex_unlock(&mtx);
	if (counter == 0)
		return -1;
	return counter;
}

void killed_unblock(struct list *node) {
	struct list *current;
	int i = 0;

	pthread_mutex_lock(&mtx);

	if(node->is_blocked==0){
		pthread_mutex_unlock(&mtx);
		return;
	}

	for(i=0;i<num_of_cores;i++){
		for (current = head[i]->next; current != head[i]; current = current->next) {
			if((current->is_blocked > node->is_blocked) && (strcmp(current->variable_blocked, node->variable_blocked) == 0)){
				current->is_blocked--;
			}
		}
	}
	pthread_mutex_unlock(&mtx);
	return;
}

struct list *remove_node (struct list *node, int id) {
	struct list *current, *previous;
	pthread_mutex_lock(&mtx);
	for (previous = head[id], current = head[id]->next; current != node; previous = current, current = current->next) {}
	if (current == head[id]){
		pthread_mutex_unlock(&mtx);
		return(NULL);
	}

	previous->next = current->next;
	free(current);

	if (previous->next == head[id])
		previous = previous->next;
	pthread_mutex_unlock(&mtx);
	return(previous->next);
}


struct list *add_node(FILE* input_fd,int id) {
	struct list *new_node;
	int j=0;
	pthread_mutex_lock(&mtx);
	new_node = (struct list *)malloc(sizeof(struct list));
	if (new_node == NULL) {
		printf("Problem with add node\n");
		pthread_mutex_unlock(&mtx);
		return NULL;
	}

	new_node->fd = input_fd;
	new_node->LocalVar = (table_info *)malloc(0);
	if(new_node->LocalVar == NULL){
		printf("ERROR with malloc at line %d\n", __LINE__);
	}
	new_node->LocalVar_size = 0;
	new_node->timetostop = 0;
	new_node->is_blocked = 0;
	new_node->is_killed = 0;
	for(j=0;j<LINESIZE-1;j++){
		new_node->variable_blocked[j] = '\0';
	}
	for(j=0;j<BUFFERSIZE-1;j++){
		new_node->filename[j] = '\0';
	}
	new_node->next = head[id]->next;

	head[id]->next = new_node;
	pthread_mutex_unlock(&mtx);
	return(new_node);
}

struct list *next_node(struct list *cur_node,int id) {
	struct list *current;

	pthread_mutex_lock(&mtx);
	current = cur_node->next;
	if(current==head[id]){
		current = current->next;
	}
	pthread_mutex_unlock(&mtx);
	return(current);
}


void destroy_list(int id) {
	free(head[id]);
}

int kill_node(int id) {
	struct list *current;
	int i;

	pthread_mutex_lock(&mtx);
	for(i=0;i<num_of_cores;i++){
		for (current = head[i]->next; current != head[i]; current = current->next) {
			if(current->LocalVar[0].value == id){
				current->is_killed = 1;
				pthread_mutex_unlock(&mtx);
				return 1;
			}
		}
	}
	pthread_mutex_unlock(&mtx);
	return -1;
}

void *system_foo(void *arg){
	char *input_filename;
	char buffer[BUFFERSIZE];
	char line_buffer[LINESIZE];
	char copybuf[LINESIZE];
	char array_name[LINESIZE];
	char token_copy[LINESIZE];
	char final_array[LINESIZE];
	char name_with_bracket[LINESIZE];
	char name_to_search[BUFFERSIZE];
	char pos_string[BUFFERSIZE];
	const char delimiter[2] = " ";
	const char bracket_del[2] = "\"";
	FILE* input_fd;
	char *pos;
	char *token, *tok, *macro_token;
	int i = 0, j = 0, k = 0, macro_i=0, macro_j=0, counter = 0, local_pos=0,global_pos=0, local_pos1=0, local_pos2=0;
	int val1=0, val2=0;
	int array_pos = 0;            //(int)position of array
	char array_pos_char[20];      //(string)position of array
	int line_counter = 1;
	int timer = 0;
	int seconds;
	struct timeval tv;
	int retval;
	fd_set rfds;
	int app_threads = 0;
	struct list *cur_node;
	int lines_read=0;
	int res_blocked = 0;
	int to_continue = 0;
	int core_id = *(int *)arg;
	int res = 0;
	int to_continue2=0;

	while(1){
		i = 0;
		j = 0;
		macro_i = 0;
		macro_j = 0;
		line_counter = 1;

		if(find_core() == core_id){
			if(to_continue == 0){

				FD_ZERO(&rfds);
				FD_SET(0, &rfds);

				tv.tv_sec = 0;
				tv.tv_usec = 1;
				retval = select(1, &rfds, NULL, NULL, &tv);
			}
			to_continue = 0;

			if (retval == -1)
				perror("select()");
			else if (retval || app_threads == 0){								//Data is available now.
				fgets(buffer, BUFFERSIZE, stdin);

				if(buffer[strlen(buffer)-1] == '\n'){
					buffer[strlen(buffer)-1] = '\0';
				}
				if(buffer[0]=='\0'){
					printf("Please enter \"run <filename>\", \"list\", \"kill <id>\" or \"exit\"\n");
					continue;
				}
				/* get the first token */
				pthread_mutex_lock(&mtx2);
				tok = strtok(buffer, delimiter);

				if(strcmp(tok, "exit")==0){
					pthread_mutex_unlock(&mtx2);
					exit(1);
				}
				else if(strcmp(tok, "list")==0){
					print_list();
					pthread_mutex_unlock(&mtx2);
					continue;
				}
				else if(strcmp(tok, "kill")==0){
					tok = strtok(NULL, delimiter);
					if(tok == NULL){
						printf("kill:Wrong number of arguments\n");
						pthread_mutex_unlock(&mtx2);
						continue;
					}
					res = kill_node(atoi(tok));
					if(res == -1){
						printf("Process:%d not found\n", atoi(tok));
					}
					else{
						printf("Process:%d killed\n", atoi(tok));
					}
					pthread_mutex_unlock(&mtx2);
					continue;
				}
				else if(strcmp(tok, "run")!=0){
					printf("Please enter \"run <filename>\", \"list\", \"kill <id>\" or \"exit\"\n");
					pthread_mutex_unlock(&mtx2);
					continue;
				}
				app_threads++;

				/* walk through other toks */
				while( tok != NULL ) {
					if(i == 1){
						input_filename = tok;
						input_fd = fopen(input_filename,"r");
						if(input_fd == NULL){
							printf("File %s does not exist\n", input_filename);
							app_threads--;
							pthread_mutex_unlock(&mtx2);
							exit(1);
						}

						cur_node = add_node(input_fd,core_id);
						if (cur_node == NULL) {
							printf("Problem in add_node\n");
							pthread_mutex_unlock(&mtx2);
							return NULL;
						}
						strcpy(cur_node->filename,input_filename);

						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, "$argv[0]", local_pos)
						cur_node->LocalVar[local_pos].value = id;
						id++;

					}
					else if(i > 1){
						strcpy(name_to_search,"$argv");
						sprintf(pos_string, "%d", i-1);
						strcat(name_to_search,"[");
						strcpy(name_with_bracket,name_to_search);
						strcat(name_to_search,pos_string);
						strcat(name_to_search,"]");
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, name_to_search, local_pos)
						cur_node->LocalVar[local_pos].value = atoi(tok);
					}
					tok = strtok(NULL, delimiter);
					if((i==0) && (tok == NULL)){
						printf("run:Wrong number of arguments\n");
						app_threads--;
						pthread_mutex_unlock(&mtx2);
						to_continue2 = 1;
						break;
					}
					i++;
				}

				if(to_continue2 == 1){
					to_continue2 = 0;
					continue;
				}

				create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, "$argc", local_pos)
				cur_node->LocalVar[local_pos].value = i-1;

				fgets(line_buffer, LINESIZE, cur_node->fd);

				if(line_buffer[strlen(line_buffer)-1] == '\n'){
					line_buffer[strlen(line_buffer)-1] = '\0';
				}
				while ((pos=strchr(line_buffer, '\r')) != NULL){		// carriage return
					*pos = '\0';
				}
				while ((pos=strchr(line_buffer, '\t')) != NULL){		// convert tabs to spaces
					*pos = ' ';
				}
				if(line_buffer[0]=='\0'){
					pthread_mutex_unlock(&mtx2);
					break;
				}
				if(strcmp(line_buffer, "#PROGRAM")!=0){
					printf("Input file has wrong Program Tag\n");
					app_threads--;
					cur_node=remove_node(cur_node, core_id);
					pthread_mutex_unlock(&mtx2);
					continue;
				}
				pthread_mutex_unlock(&mtx2);

				while(1){
					pthread_mutex_lock(&mtx2);
					if(cur_node->is_killed==1){
						app_threads--;
						killed_unblock(cur_node);
						cur_node = remove_node(cur_node, core_id);
						to_continue2 = 1;
						pthread_mutex_unlock(&mtx2);
						break;
					}
					if(fgets(line_buffer, LINESIZE, cur_node->fd)==NULL){
						pthread_mutex_unlock(&mtx2);
						break;
					}
					if(line_buffer[strlen(line_buffer)-1] == '\n'){
						line_buffer[strlen(line_buffer)-1] = '\0';
					}

					while ((pos=strchr(line_buffer, '\r')) != NULL){		// carriage return
						*pos = '\0';
					}
					while ((pos=strchr(line_buffer, '\t')) != NULL){		// convert tabs to spaces
						*pos = ' ';
					}
					if(line_buffer[0]=='\0'){
						pthread_mutex_unlock(&mtx2);
						break;
					}

					token = strtok(line_buffer, delimiter);
					if((token[0] == 'L') && (strcmp(token, "LOAD")!=0)){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos)
						cur_node->LocalVar[local_pos].value = line_counter;
					}
					line_counter++;
					pthread_mutex_unlock(&mtx2);
				}

				if(to_continue2 == 1){
					to_continue2 = 0;
					continue;
				}
				fseek(cur_node->fd, 0, SEEK_SET);
				fgets(line_buffer, LINESIZE, cur_node->fd);		// skip the #PROGRAM
			}
		}

		if(app_threads == 0){
			continue;
		}

		seconds = time(NULL);

		while((cur_node->timetostop > seconds) || (lines_read == MAX_LINES) || (cur_node->is_blocked > 0)){
			if(find_core() == core_id){
				FD_ZERO(&rfds);
				FD_SET(0, &rfds);

				tv.tv_sec = 0;
				tv.tv_usec = 1;
				retval = select(1, &rfds, NULL, NULL, &tv);
				if(retval){
					to_continue = 1;
					break;
				}
			}
			if(cur_node->is_killed == 1){
				pthread_mutex_lock(&mtx2);
				app_threads--;
				killed_unblock(cur_node);
				cur_node = remove_node(cur_node, core_id);
				to_continue2 = 1;
				pthread_mutex_unlock(&mtx2);
				break;
			}
			seconds = time(NULL);
			lines_read = 0;
			cur_node = next_node(cur_node, core_id);
		}

		if(to_continue == 1){
			continue;
		}
		if(to_continue2 == 1){
			to_continue2 = 0;
			continue;
		}
		if(fgets(line_buffer, LINESIZE, cur_node->fd)==NULL){
			app_threads--;
			cur_node=remove_node(cur_node, core_id);
			continue;
		}
		if(line_buffer[strlen(line_buffer)-1] == '\n'){
			line_buffer[strlen(line_buffer)-1] = '\0';
		}

		while ((pos=strchr(line_buffer, '\r')) != NULL){		// carriage return
			*pos = '\0';
		}
		while ((pos=strchr(line_buffer, '\t')) != NULL){		// convert tabs to spaces
			*pos = ' ';
		}
		if(line_buffer[0]=='\0')
			break;
		counter = 0;
		strcpy(copybuf,line_buffer);
		/* get the first token */
		/////////////////////////
		pthread_mutex_lock(&mtx2);
		/////////////////////////
		if(cur_node->is_killed==1){
			app_threads--;
			killed_unblock(cur_node);
			cur_node = remove_node(cur_node, core_id);
			pthread_mutex_unlock(&mtx2);
			continue;
		}
		token = strtok(line_buffer, delimiter);
		if((token[0] == 'L') && (strcmp(token, "LOAD")!=0)){
			strcpy(copybuf,&copybuf[strlen(token)]);
			token = strtok(NULL, delimiter);
		}
		if(token == NULL){
			pthread_mutex_unlock(&mtx2);
			continue;
		}
		lines_read++;
		// GlobalVar
		if(strcmp(token, "LOAD")==0){
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token != NULL){
				printf("Syntax error with LOAD: too many arguments\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with LOAD 1st argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(token_copy, token);
			// check if token is array
			tok = strtok(token, "[]");
			if (strcmp(tok, token_copy) != 0){
				search_array(array_name, final_array, array_pos, array_pos_char, local_pos, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
			}
			else{
				strcpy(token, token_copy);
				local_pos =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
				if(local_pos == -1){
					create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos)
				}
			}
			// GlobalVar
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with LOAD 2nd argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(token_copy, token);
			tok = strtok(token, "[]");
			if (strcmp(tok, token_copy) != 0){
				search_array(array_name, final_array, array_pos, array_pos_char, global_pos, GlobalVar, GlobalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
			}
			else{
				strcpy(token, token_copy);
				global_pos =  search_var(GlobalVar,&GlobalVar_size, token,0);
				if(global_pos == -1){
					create_new_variable(GlobalVar, GlobalVar_size, token, global_pos)
				}
			}
			cur_node->LocalVar[local_pos].value = GlobalVar[global_pos].value;
		}
		else if(strcmp(token, "STORE")==0){
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token != NULL){
				printf("Syntax error with STORE: too many arguments\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			// Global Var
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with STORE 1st argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(token_copy, token);
			tok = strtok(token, "[]");
			if (strcmp(tok, token_copy) != 0){
				search_array(array_name, final_array, array_pos, array_pos_char, global_pos, GlobalVar, GlobalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
			}
			else{
				strcpy(token, token_copy);
				global_pos =  search_var(GlobalVar,&GlobalVar_size, token,0);
				if(global_pos == -1){
					create_new_variable(GlobalVar, GlobalVar_size, token, global_pos)
				}
			}
			// Local Var
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with STORE 2nd argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos)
					}
				}

				GlobalVar[global_pos].value = cur_node->LocalVar[local_pos].value;
			}
			else{
				GlobalVar[global_pos].value = atoi(token);
			}
		}
		else if(strcmp(token, "SET")==0){
			// var
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with SET 1st argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(token_copy, token);
			// check if token is array
			tok = strtok(token, "[]");
			if (strcmp(tok, token_copy) != 0){
				search_array(array_name, final_array, array_pos, array_pos_char, local_pos1, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
			}
			else{
				strcpy(token, token_copy);
				local_pos1 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
				if(local_pos1 == -1){
					create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos1)
				}
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			//varval
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with SET 2nd argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos2, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos2 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos2 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos2)
					}
				}

				cur_node->LocalVar[local_pos1].value = cur_node->LocalVar[local_pos2].value;
			}
			else{
				cur_node->LocalVar[local_pos1].value = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token != NULL){
				printf("Syntax error with SET: too many arguments\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
		}
		else if(strcmp(token, "ADD")==0){
			// var
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with ADD 1st argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(token_copy, token);
			// check if token is array
			tok = strtok(token, "[]");
			if (strcmp(tok, token_copy) != 0){
				search_array(array_name, final_array, array_pos, array_pos_char, local_pos, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
			}
			else{
				strcpy(token, token_copy);
				local_pos =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
				if(local_pos == -1){
					create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos)
				}
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			//varval1
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with ADD 2nd argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos1, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos1 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos1 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos1)
					}
				}
				val1 = cur_node->LocalVar[local_pos1].value;
			}
			else{
				val1 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			//varval2
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with ADD 3rd argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos2, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos2 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos2 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos2)
					}
				}

				val2 = cur_node->LocalVar[local_pos2].value;
			}
			else{
				val2 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token != NULL){
				printf("Syntax error with ADD: too many arguments\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			cur_node->LocalVar[local_pos].value = val1 + val2;
		}
		else if(strcmp(token, "SUB")==0){
			// var
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with SUB 1st argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(token_copy, token);
			// check if token is array
			tok = strtok(token, "[]");
			if (strcmp(tok, token_copy) != 0){
				search_array(array_name, final_array, array_pos, array_pos_char, local_pos, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
			}
			else{
				strcpy(token, token_copy);
				local_pos =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
				if(local_pos == -1){
					create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos)
				}
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			//varval1
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with SUB 2nd argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos1, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos1 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos1 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos1)
					}
				}

				val1 = cur_node->LocalVar[local_pos1].value;
			}
			else{
				val1 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			//varval2
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with SUB 3rd argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos2, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos2 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos2 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos2)
					}
				}

				val2 = cur_node->LocalVar[local_pos2].value;
			}
			else{
				val2 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token != NULL){
				printf("Syntax error with SUB: too many arguments\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			cur_node->LocalVar[local_pos].value = val1 - val2;
		}
		else if(strcmp(token, "MUL")==0){
			// var
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with MUL 1st argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(token_copy, token);
			// check if token is array
			tok = strtok(token, "[]");
			if (strcmp(tok, token_copy) != 0){
				search_array(array_name, final_array, array_pos, array_pos_char, local_pos, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
			}
			else{
				strcpy(token, token_copy);
				local_pos =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
				if(local_pos == -1){
					create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos)
				}
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			//varval1
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with MUL 2nd argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos1, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos1 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos1 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos1)
					}
				}

				val1 = cur_node->LocalVar[local_pos1].value;
			}
			else{
				val1 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			//varval2
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with MUL 3rd argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos2, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos2 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos2 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos2)
					}
				}

				val2 = cur_node->LocalVar[local_pos2].value;
			}
			else{
				val2 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token != NULL){
				printf("Syntax error with MUL: too many arguments\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			cur_node->LocalVar[local_pos].value = val1 * val2;
		}
		else if(strcmp(token, "DIV")==0){
			// var
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with DIV 1st argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(token_copy, token);
			// check if token is array
			tok = strtok(token, "[]");
			if (strcmp(tok, token_copy) != 0){
				search_array(array_name, final_array, array_pos, array_pos_char, local_pos, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
			}
			else{
				strcpy(token, token_copy);
				local_pos =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
				if(local_pos == -1){
					create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos)
				}
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			//varval1
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with DIV 2nd argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos1, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos1 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos1 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos1)
					}
				}

				val1 = cur_node->LocalVar[local_pos1].value;
			}
			else{
				val1 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			//varval2
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with DIV 3rd argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos2, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos2 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos2 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos2)
					}
				}

				val2 = cur_node->LocalVar[local_pos2].value;
			}
			else{
				val2 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token != NULL){
				printf("Syntax error with DIV: too many arguments\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			cur_node->LocalVar[local_pos].value = val1 / val2;
		}
		else if(strcmp(token, "MOD")==0){
			// var
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with MOD 1st argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(token_copy, token);
			// check if token is array
			tok = strtok(token, "[]");
			if (strcmp(tok, token_copy) != 0){
				search_array(array_name, final_array, array_pos, array_pos_char, local_pos, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
			}
			else{
				strcpy(token, token_copy);
				local_pos =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
				if(local_pos == -1){
					create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos)
				}
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			//varval1
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with MOD 2nd argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos1, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos1 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos1 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos1)
					}
				}

				val1 = cur_node->LocalVar[local_pos1].value;
			}
			else{
				val1 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			//varval2
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with MOD 3rd argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos2, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos2 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos2 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos2)
					}
				}

				val2 = cur_node->LocalVar[local_pos2].value;
			}
			else{
				val2 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token != NULL){
				printf("Syntax error with MOD: too many arguments\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			cur_node->LocalVar[local_pos].value = val1 % val2;
		}
		else if(strcmp(token, "BRGT")==0){
			//varval1
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with BRGT 1st argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos1, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos1 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos1 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos1)
					}
				}

				val1 = cur_node->LocalVar[local_pos1].value;
			}
			else{
				val1 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			//varval2
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with BRGT 2nd argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos2, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos2 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos2 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos2)
					}
				}

				val2 = cur_node->LocalVar[local_pos2].value;
			}
			else{
				val2 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token != NULL){
				printf("Syntax error with BRGT: too many arguments\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(val1 > val2){
				token = strtok(NULL, delimiter);
				if(token == NULL){
					printf("Syntax error with BRGT 3rd argument\n");
					app_threads--;
					cur_node = remove_node(cur_node, core_id);
					pthread_mutex_unlock(&mtx2);
					continue;
				}
				local_pos = search_var(cur_node->LocalVar, &cur_node->LocalVar_size, token, 0);
				fseek(cur_node->fd, 0, SEEK_SET);
				for(i=0; i<cur_node->LocalVar[local_pos].value; i++){
					fgets(line_buffer, LINESIZE, cur_node->fd);
				}
			}
		}
		else if(strcmp(token, "BRGE")==0){
			//varval1
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with BRGE 1st argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos1, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos1 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos1 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos1)
					}
				}

				val1 = cur_node->LocalVar[local_pos1].value;
			}
			else{
				val1 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			//varval2
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with BRGE 2nd argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos2, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos2 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos2 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos2)
					}
				}

				val2 = cur_node->LocalVar[local_pos2].value;
			}
			else{
				val2 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token != NULL){
				printf("Syntax error with BRGE: too many arguments\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(val1 >= val2){
				token = strtok(NULL, delimiter);
				if(token == NULL){
					printf("Syntax error with BRGE 3rd argument\n");
					app_threads--;
					cur_node = remove_node(cur_node, core_id);
					pthread_mutex_unlock(&mtx2);
					continue;
				}
				local_pos = search_var(cur_node->LocalVar, &cur_node->LocalVar_size, token, 0);
				fseek(cur_node->fd, 0, SEEK_SET);
				for(i=0; i<cur_node->LocalVar[local_pos].value; i++){
					fgets(line_buffer, LINESIZE, cur_node->fd);
				}
			}

		}
		else if(strcmp(token, "BRLT")==0){
			//varval1
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with BRLT 1st argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos1, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos1 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos1 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos1)
					}
				}

				val1 = cur_node->LocalVar[local_pos1].value;
			}
			else{
				val1 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			//varval2
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with BRLT 2nd argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos2, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos2 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos2 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos2)
					}
				}

				val2 = cur_node->LocalVar[local_pos2].value;
			}
			else{
				val2 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token != NULL){
				printf("Syntax error with BRLT: too many arguments\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(val1 < val2){
				token = strtok(NULL, delimiter);
				if(token == NULL){
					printf("Syntax error with BRLT 3rd argument\n");
					app_threads--;
					cur_node = remove_node(cur_node, core_id);
					pthread_mutex_unlock(&mtx2);
					continue;
				}
				local_pos = search_var(cur_node->LocalVar, &cur_node->LocalVar_size, token, 0);
				fseek(cur_node->fd, 0, SEEK_SET);
				for(i=0; i<cur_node->LocalVar[local_pos].value; i++){
					fgets(line_buffer, LINESIZE, cur_node->fd);
				}
			}
		}
		else if(strcmp(token, "BRLE")==0){
			//varval1
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with BRLE 1st argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos1, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos1 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos1 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos1)
					}
				}

				val1 = cur_node->LocalVar[local_pos1].value;
			}
			else{
				val1 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			//varval2
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with BRLE 2nd argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos2, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos2 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos2 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos2)
					}
				}

				val2 = cur_node->LocalVar[local_pos2].value;
			}
			else{
				val2 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token != NULL){
				printf("Syntax error with BRLE: too many arguments\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(val1 <= val2){
				token = strtok(NULL, delimiter);
				if(token == NULL){
					printf("Syntax error with BRLE 3rd argument\n");
					app_threads--;
					cur_node = remove_node(cur_node, core_id);
					pthread_mutex_unlock(&mtx2);
					continue;
				}
				local_pos = search_var(cur_node->LocalVar, &cur_node->LocalVar_size, token, 0);
				fseek(cur_node->fd, 0, SEEK_SET);
				for(i=0; i<cur_node->LocalVar[local_pos].value; i++){
					fgets(line_buffer, LINESIZE, cur_node->fd);
				}
			}
		}
		else if(strcmp(token, "BREQ")==0){
			//varval1
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with BREQ 1st argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos1, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos1 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos1 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos1)
					}
				}

				val1 = cur_node->LocalVar[local_pos1].value;
			}
			else{
				val1 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			//varval2
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with BREQ 2nd argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos2, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos2 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos2 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos2)
					}
				}

				val2 = cur_node->LocalVar[local_pos2].value;
			}
			else{
				val2 = atoi(token);
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token != NULL){
				printf("Syntax error with BREQ: too many arguments\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);

			if(val1 == val2){
				token = strtok(NULL, delimiter);
				if(token == NULL){
					printf("Syntax error with BREQ 3rd argument\n");
					app_threads--;
					cur_node = remove_node(cur_node, core_id);
					pthread_mutex_unlock(&mtx2);
					continue;
				}
				local_pos = search_var(cur_node->LocalVar, &cur_node->LocalVar_size, token, 0);
				fseek(cur_node->fd, 0, SEEK_SET);
				for(i=0; i<cur_node->LocalVar[local_pos].value; i++){
					fgets(line_buffer, LINESIZE, cur_node->fd);
				}
			}
		}
		else if(strcmp(token, "BRA")==0){
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token != NULL){
				printf("Syntax error with BRA: too many arguments\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with BRA 1st argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			local_pos = search_var(cur_node->LocalVar, &cur_node->LocalVar_size, token, 0);
			fseek(cur_node->fd, 0, SEEK_SET);
			for(i=0; i<cur_node->LocalVar[local_pos].value; i++){
				fgets(line_buffer, LINESIZE, cur_node->fd);
			}
		}
		else if(strcmp(token, "DOWN")==0){
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token != NULL){
				printf("Syntax error with DOWN: too many arguments\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with DOWN 1st argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(token_copy, token);
			tok = strtok(token, "[]");
			if (strcmp(tok, token_copy) != 0){
				search_array(array_name, final_array, array_pos, array_pos_char, global_pos, GlobalVar, GlobalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
			}
			else{
				strcpy(token, token_copy);
				global_pos =  search_var(GlobalVar,&GlobalVar_size, token,0);
				if(global_pos == -1){
					create_new_variable(GlobalVar, GlobalVar_size, token, global_pos)
				}
			}
			if(GlobalVar[global_pos].value == 0){
				block(token, cur_node);
			}
			else{
				GlobalVar[global_pos].value--;
			}
		}
		else if(strcmp(token, "UP")==0){
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token != NULL){
				printf("Syntax error with UP: too many arguments\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with UP 1st argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(token_copy, token);
			tok = strtok(token, "[]");
			if (strcmp(tok, token_copy) != 0){
				search_array(array_name, final_array, array_pos, array_pos_char, global_pos, GlobalVar, GlobalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
			}
			else{
				strcpy(token, token_copy);
				global_pos =  search_var(GlobalVar,&GlobalVar_size, token,0);
				if(global_pos == -1){
					create_new_variable(GlobalVar, GlobalVar_size, token, global_pos)
				}
			}
			if(GlobalVar[global_pos].value == 0){
				res_blocked = unblock(token);
				if(res_blocked == -1){
					GlobalVar[global_pos].value++;
				}
			}
			else{
				GlobalVar[global_pos].value++;
			}
		}
		else if(strcmp(token, "SLEEP")==0){
			timer = 0;
			cur_node->timetostop = 0;
			// varval
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			token = strtok(NULL, delimiter);
			if(token != NULL){
				printf("Syntax error with SLEEP: too many arguments\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			if(token == NULL){
				printf("Syntax error with SLEEP 1st argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			if(token[0] == '$'){
				strcpy(token_copy, token);
				tok = strtok(token, "[]");
				if (strcmp(tok, token_copy) != 0){
					search_array(array_name, final_array, array_pos, array_pos_char, local_pos1, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
				}
				else{
					strcpy(token, token_copy);
					local_pos1 =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
					if(local_pos1 == -1){
						create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos1)
					}
				}

				val1 = cur_node->LocalVar[local_pos1].value;
			}
			else{
				val1 = atoi(token);
			}
			timer = val1;
			cur_node->timetostop = time(NULL);
			cur_node->timetostop += timer;
			cur_node = next_node(cur_node, core_id);
		}
		else if(strcmp(token, "PRINT")==0){
			printf("Thread %d: ", cur_node->LocalVar[0].value); 							//thread id
			strcpy(line_buffer,copybuf);
			token = strtok(line_buffer, bracket_del); 	//PRINT
			token = strtok(NULL, bracket_del);			//String
			if(token == NULL){
				printf("Syntax error with PRINT 1st argument\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			printf("%s",token);

			token = strtok(NULL, bracket_del);
			if(token!=NULL){
				strcpy(line_buffer,&token[1]);
				strcpy(copybuf, line_buffer);
				token = strtok(line_buffer, delimiter);
			}
			while( token != NULL ) {
				if(token[0] == '$'){
					strcpy(token_copy, token);
					tok = strtok(token, "[]");
					if (strcmp(tok, token_copy) != 0){
						search_array(array_name, final_array, array_pos, array_pos_char, local_pos, cur_node->LocalVar, cur_node->LocalVar_size,macro_i,macro_j,name_with_bracket,macro_token, tok)
					}
					else{
						strcpy(token, token_copy);
						local_pos =  search_var(cur_node->LocalVar,&cur_node->LocalVar_size, token,0);
						if(local_pos == -1){
							create_new_variable(cur_node->LocalVar, cur_node->LocalVar_size, token, local_pos)
						}
					}
					printf(" %d", cur_node->LocalVar[local_pos].value);
				}
				else{
					strcpy(token, token_copy);
					printf(" %d", atoi(token));
				}
				strcpy(line_buffer,copybuf);
				token = strtok(line_buffer, delimiter);
				token = strtok(NULL, delimiter);
				for(k=0; k<counter; k++){
					token = strtok(NULL, delimiter);
				}
				counter++;
			}
			printf("\n");
		}

		else if(strcmp(token, "RETURN")==0){
			strcpy(line_buffer, copybuf);
			token = strtok(line_buffer, delimiter);
			token = strtok(NULL, delimiter);
			if(token != NULL){
				printf("Syntax error with RETURN: too many arguments\n");
				app_threads--;
				cur_node = remove_node(cur_node, core_id);
				pthread_mutex_unlock(&mtx2);
				continue;
			}
			app_threads--;
			cur_node = remove_node(cur_node, core_id);
			pthread_mutex_unlock(&mtx2);
			continue;
		}

		for(j=0;j<LINESIZE-1;j++){
			line_buffer[j] = '\0';
		}

		pthread_mutex_unlock(&mtx2);
	}

	return NULL;
}


int main(int argc,char *argv[]){
	int res;
	pthread_t *system_thread;
	int i;
	int *core_number;
	int  mtxtype = PTHREAD_MUTEX_NORMAL;
	pthread_mutexattr_t attr;

	if(argc != 2){
		printf("Please provide the number of system threads");
		return 1;
	}

	num_of_cores = atoi(argv[1]);

	system_thread = (pthread_t *)malloc(sizeof(pthread_t) * (num_of_cores));
	core_number = (int *)malloc(sizeof(int) * (num_of_cores));
	head = (struct list **)malloc((sizeof(struct list*))*num_of_cores);
	if (head == NULL) {
		printf("Memory allocation error in initialization\n");
		exit(1);
	}

	GlobalVar = (table_info *) malloc(0);
	if(GlobalVar == NULL){
		printf("ERROR with malloc at line %d\n", __LINE__);
	}


	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, mtxtype);
	pthread_mutex_init(&mtx, &attr);
	pthread_mutex_init(&mtx2, &attr);

	for(i=0;i<num_of_cores;i++){
		init_list(i);
	}

	for(i=0;i<num_of_cores;i++){
		core_number[i]=i;
		res = pthread_create(&system_thread[i], NULL, system_foo, (void*)&core_number[i]);
		if(res){
			printf("Error with the system thread creation");
			exit(1);
		}
	}

	for(i=0;i<num_of_cores;i++){
		res = pthread_join(system_thread[i], NULL);
		if(res){
			printf("Error with thread join number %d", i);
			exit(1);
		}
	}

	return(0);
}
