#define CCR_DECLARE(label) struct_label label;

#define CCR_INIT(label) 								\
	int  mtxtype = PTHREAD_MUTEX_NORMAL;				\
	pthread_mutexattr_t attr;							\
														\
	pthread_mutexattr_init(&attr);						\
	pthread_mutexattr_settype(&attr, mtxtype);			\
	pthread_mutex_init(&label.mtx, &attr);				\
														\
	pthread_cond_init(&label.q1, NULL);					\
	pthread_cond_init(&label.q2, NULL);					\
														\
	label.n1 = 0;										\
	label.n2 = 0;

#define CCR_EXEC(label,cond,body) 						\
	pthread_mutex_lock(&label.mtx);						\
	while(!(cond)){										\
		label.n1++;										\
		if(label.n2>0){									\
			label.n2--;									\
			pthread_cond_signal(&label.q2);				\
		}												\
		pthread_cond_wait(&label.q1, &label.mtx);		\
		label.n2++;										\
		if(label.n1>0){									\
			label.n1--;									\
			pthread_cond_signal(&label.q1);				\
			pthread_cond_wait(&label.q2, &label.mtx);	\
		}												\
		else if(label.n2>1){							\
			label.n2--;									\
			pthread_cond_signal(&label.q2);				\
			pthread_cond_wait(&label.q2, &label.mtx);	\
		} 												\
	}													\
														\
	body												\
														\
	if(label.n1>0){										\
		label.n1--;										\
		pthread_cond_signal(&label.q1);					\
	}													\
	else if(label.n2>0){								\
		label.n2--;										\
		pthread_cond_signal(&label.q2);					\
	}													\
	pthread_mutex_unlock(&label.mtx);

typedef struct labels{
	int n1;
	int n2;
	pthread_cond_t q1;
	pthread_cond_t q2;
	pthread_mutex_t mtx;
}struct_label;

