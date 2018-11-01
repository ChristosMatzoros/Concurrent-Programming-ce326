/* Multithreaded QuickSort */
#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

struct info{
	int array_size;
	int low;	// Starting index
	int high;	// Ending index
	int done;	// flag that is set to 1 when the current thread finishes
};

volatile int *arr;
void printArray(int size);

// A utility function to swap two elements
void swap(volatile int* a, volatile int* b){
	int t = *a;
	*a = *b;
	*b = t;
}

/* This function takes last element as pivot, places
the pivot element at its correct position in sorted
array, and places all smaller (smaller than pivot)
to left of pivot and all greater elements to right
of pivot */
int partition (int low, int high){
	int pivot = arr[high]; // pivot
	int i = (low - 1); // Index of smaller element
	int j;
	for (j = low; j <= high- 1; j++){
		// If current element is smaller than or
		// equal to pivot
		if (arr[j] <= pivot){
			i++; // increment index of smaller element
			swap(&arr[i], &arr[j]);
		}
	}
	swap(&arr[i + 1], &arr[high]);
	return (i + 1);
}

/* The main function that implements QuickSort
arr[] --> Array to be sorted,
low --> Starting index,
high --> Ending index */
void *quickSort(void *arg){	
	pthread_t left_thread,right_thread;
	int pi,res1,res2;
	struct info *mine = (struct info *)arg;
	struct info left, right;
	
	if(mine->low < mine->high){
		/* pi is partitioning index, arr[p] is now
		at right place */
		pi = partition(mine->low, mine->high);
		
		// Separately sort elements before
		// partition and after partition
		
		//First thread
		left.array_size = mine->array_size;
		left.done=0;
		left.low = mine->low;
		left.high = pi - 1;
		
		
		res1 = pthread_create(&left_thread,NULL,quickSort,(void *)&left);
		if(res1){
			printf("Error with the left thread creation\n");
			exit(1);
		}
		
		//Second thread
		right.array_size = mine->array_size;
		right.done=0;
		right.low = pi + 1;
		right.high = mine->high;
		
		res2 = pthread_create(&right_thread,NULL,quickSort,(void *)&right);
		if(res2){
			printf("Error with the right thread creation\n");
			exit(1);
		}
		
		// wait for both "child" threads to finish
		while(left.done !=1 || right.done !=1){
			pthread_yield();
		}
	}
	// change flag to 1 to let the "parent" know that the thread finished
	mine->done = 1;
	return NULL;
}

/* Function to print an array */
void printArray(int size){
	int i;
	
	for (i=0; i < size; i++)
		printf("%d ", arr[i]);
	printf("\n");
}

// Driver program to test above functions
int main(int argc,char *argv[]){
	pthread_t main_thread1, main_thread2;
	int res1, res2, low, high, pi, arr_size, i;
	struct info left, right;
	
	
	printf("Enter the number of elements to be sorted\n");
	scanf("%d", &arr_size);
	if(arr_size < 1){
		printf("There must be at least 1 element to be sorted.\n");
		exit(1);
	}
	
	arr = (int *)malloc(sizeof(int) * arr_size);
	
	printf("Enter the elements\n");
	for(i=0; i<arr_size; i++){
		scanf("%d", &arr[i]);
	}
	
	low = 0;
	high = arr_size - 1;
	
	if(low < high){
		/* pi is partitioning index, arr[p] is now
		at right place */
		pi = partition(low, high);
		
		// Separately sort elements before
		// partition and after partition
		
		//First thread
		left.array_size = arr_size;
		left.done = 0;
		left.low = low;
		left.high = pi - 1;
		
		res1 = pthread_create(&main_thread1,NULL,quickSort,(void *)&left);
		if(res1){
			printf("Error with the left thread created by main\n");
			exit(1);
		}
		
		//Second thread
		right.array_size = arr_size;
		right.done = 0;
		right.low = pi + 1;
		right.high = high;
		
		res2 = pthread_create(&main_thread2,NULL,quickSort,(void *)&right);
		if(res2){
			printf("Error with the right thread created by main\n");
			exit(1);
		}
		// wait for both "child" threads to finish
		while(left.done !=1 || right.done !=1){
			pthread_yield();
		}
	}
	
	printf("Sorted array: \n");
	printArray(arr_size);
	return 0;
}