// mybsem data type
typedef struct semaphore{
	int flag;
	int is_blocked;
	int val;
	pthread_mutex_t mtx;
}mybsem;

// Semaphore Initiatization
void mybsem_init(mybsem *sem, int init_value){
	int  mtxtype = PTHREAD_MUTEX_NORMAL;
	pthread_mutexattr_t attr;
	int res;
	
	if((init_value != 0) && (init_value != 1)){
		printf("Init value is neither 0 nor 1\n");
		exit(0);
	}
	
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, mtxtype);
	pthread_mutex_init(&sem->mtx, &attr);
	
	sem->flag = init_value;
	sem->val = init_value;
	sem->is_blocked = 0;
	
	if(init_value == 0){
		res = pthread_mutex_lock(&sem->mtx);
		if(res){
			printf("ERROR\n");
			exit(0);
		}
	}
	
	printf("Init value is: %d\n", sem->flag);
	printf("Semaphore Initialized\n");
	
}

// Semaphore Destruction
void mybsem_destroy(mybsem *sem){
	int res;
	
	if(sem->val == 0){
		res = pthread_mutex_unlock(&sem->mtx);
		if(res){
			printf("ERROR thread %lu: return code from pthread_mutex_unlock() at mybsem_destroy is %d %s\n", pthread_self(), res, strerror(res));
			exit(0);
		}
	}
	
	res = pthread_mutex_destroy(&sem->mtx);
	if(res){
		printf("ERROR thread %lu: return code from pthread_mutex_destroy() is %d %s\n", pthread_self(), res, strerror(res));
		exit(0);
	}
	
	printf("Semaphore Destroyed\n");
}

void mybsem_down(mybsem *sem){
	int res;
	long unsigned self = pthread_self();
	
	if(sem->flag == 0){
		sem->is_blocked ++;
		
		printf("Thread %lu entered the queue. %d threads in queue\n", self,sem->is_blocked);
		
		res = pthread_mutex_lock(&sem->mtx);
		if(res){
			printf("ERROR thread %lu: return code from pthread_mutex_lock() is %d %s\n", self, res, strerror(res));
			exit(0);
		}
		
		printf("Thread %lu exited the queue. %d threads in queue\n", self,sem->is_blocked);
	}
	else if(sem->flag == 1){
		sem->flag--;
		
		res = pthread_mutex_lock(&sem->mtx);
		if(res){
			printf("ERROR thread %lu: return code from pthread_mutex_lock() is %d %s\n", self, res, strerror(res));
			exit(0);
		}
			
		printf("Thread %lu decreased the flag value from 1 to 0\n", self);
	}
}
	
void mybsem_up(mybsem *sem){
	int res;
	long unsigned self = pthread_self();
	
	if( (sem->flag == 0) && (sem->is_blocked > 0 )){
		sem->is_blocked --;
		
		res = pthread_mutex_unlock(&sem->mtx);
		if(res){
			printf("ERROR thread %lu: return code from pthread_mutex_unlock() is %d %s\n", self, res, strerror(res));
			exit(0);
		}
		pthread_yield();
	}
	else if( (sem->flag == 0) && (sem->is_blocked == 0) ){
		res = pthread_mutex_unlock(&sem->mtx);
		if(res){
			printf("ERROR thread %lu: return code from pthread_mutex_unlock() is %d %s\n", self, res, strerror(res));
			exit(0);
		}
		
		sem->flag++;
		
		printf("Thread %lu increased the flag value from 0 to 1\n", self);
		
		
	}
	else{
		printf("Error: Trying to increase the binary semaphore value higher than one.\n");
		exit(0);
	}
}