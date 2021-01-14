#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>


///////////////////////////////////////////////////////////////////////////
// CSCI 503 Operating System
// Lab 3 - Thread Synchronization and Thread Parallelization Using Pthreads
// Part 1: Multiple Producer-Multiple Consumer (MPMC) Problem
// Author: Chu-An Tsai
///////////////////////////////////////////////////////////////////////////

// It is a parallel program that can create M producer threads and N consumer threads.
// Each thread keeps track of how many products have been produced or consumed.
// The total number of consumed products should match the total number of produced products.

// Structure declaration
struct Myownsemaphore 
{
	int value;
	int waiting_list_size;
	pthread_mutex_t mutex;
	pthread_cond_t wait;
};

struct Myownsemaphore * init_mysemaphore(int ring_buffer_size) 
{
	struct Myownsemaphore * onesemaphore = (struct Myownsemaphore *) malloc(sizeof(struct Myownsemaphore));
	onesemaphore->value = ring_buffer_size;
	pthread_cond_init(&(onesemaphore->wait),NULL);
	pthread_mutex_init(&(onesemaphore->mutex), NULL);
	
	return 0;
}

void P(struct Myownsemaphore * mysemaphore) 
{
	pthread_mutex_lock(&(mysemaphore->mutex));

	while(mysemaphore->value <= 0)
	{
		if(mysemaphore->waiting_list_size < (-1* mysemaphore->value))
		{
			__sync_fetch_and_add(&(mysemaphore->waiting_list_size),1);
		}
		pthread_cond_wait(&(mysemaphore->wait), &(mysemaphore->mutex));
	}

	mysemaphore->value --;
	pthread_mutex_unlock(&(mysemaphore->mutex));
}

void V(struct Myownsemaphore * mysemaphore)
{
	pthread_mutex_lock(&(mysemaphore->mutex));
	mysemaphore->value ++;

	if(mysemaphore->waiting_list_size > 0)
	{
		pthread_cond_broadcast(&(mysemaphore->wait));
		__sync_fetch_and_add(&(mysemaphore->waiting_list_size),-1);
	}

	if(mysemaphore->value <= 0)
	{
		pthread_cond_broadcast(&(mysemaphore->wait));
	}

	pthread_mutex_unlock(&(mysemaphore->mutex));	
}

struct Semaphore_Ring 
{
  	pthread_mutex_t mutex_lock;
  	pthread_cond_t  emptyCount;
  	pthread_cond_t  fullCount;
  	int * buffer_array;
  	unsigned head;
  	unsigned tail;
  	int ring_buffer_size;
  	int max_sleep_seconds;
  	int total_number_items2produce;
  	int num_produced_items;
  	int num_consumed_items;
};

// Initialize the semaphore structure
void initial_sema_ring (struct Semaphore_Ring * sema_buffer, int ring_buffer_size, int max_sleep_seconds, int total_number_items2produce) 
{
	sema_buffer->buffer_array = (int *) malloc(sizeof(int) * ring_buffer_size);
	sema_buffer->ring_buffer_size = ring_buffer_size;
	sema_buffer->max_sleep_seconds = max_sleep_seconds;
	sema_buffer->total_number_items2produce = total_number_items2produce;
	sema_buffer->num_produced_items = 0;
	sema_buffer->num_consumed_items = 0;
	if(pthread_mutex_init(&sema_buffer->mutex_lock, NULL)!=0)
	{
		perror("mutex init error!\n");
		exit(1);
	}
	if(pthread_cond_init(&sema_buffer->emptyCount, NULL))
	{
		perror("empty count init error!\n");
		exit(1);
	}
	if(pthread_cond_init(&sema_buffer->fullCount, NULL))
	{
		perror("fullcount init error!\n");
		exit(1);
	}
}

// Check if the ring buffer is full or not
int is_ring_full(struct Semaphore_Ring * sema_buffer)
{
	if(sema_buffer->head - sema_buffer->tail == sema_buffer->ring_buffer_size)
	{
		return 1;
	}
	else{
		return 0;
	}
}

// Check if the ring buffer is empty or not
int is_ring_empty(struct Semaphore_Ring * sema_buffer)
{
	if(sema_buffer->head - sema_buffer->tail == 0)
	{
		return 1;
	}
	else{
		return 0;
	}
}


// Add the new item of ringbuufer
int add_items_in(struct Semaphore_Ring * sema_buffer, int newitem)
{
	unsigned head = sema_buffer->head;
    unsigned tail = sema_buffer->tail;
	if(head - tail == sema_buffer->ring_buffer_size)
	{
		return -1; //buffer is full
	}
	unsigned index = head % sema_buffer->ring_buffer_size;
	sema_buffer->buffer_array[index] = newitem;

        // Atomic operation
        __sync_fetch_and_add(&(sema_buffer->head), 1);

	return 0;
}

// Remove the existing item of ringbuffer
int remove_items(struct Semaphore_Ring * sema_buffer, int *ret_item)
{
	unsigned head = sema_buffer->head;
    unsigned tail = sema_buffer->tail;
	if(head - tail == 0)
	{
		return -1; 
    }
	unsigned index = tail % sema_buffer->ring_buffer_size;
	*ret_item = sema_buffer->buffer_array[index];

		// Atomic operation
        __sync_fetch_and_add(&(sema_buffer->tail), 1);

	return 0;
}

// Determine the sleep time
int rand_sleep_seconds(int n) 
{ 
	return  ( ((double) rand()) / RAND_MAX ) * n + 1;
}

typedef struct lv_t 
{
  int             tid;
  int             related_num_items;
  void            *gv;
}* LV;

typedef struct gv_t 
{
  struct Semaphore_Ring sema_buffer;
}* GV;

// Initialize the local variable, the number of produced or consumed items
void init_lv(LV lv, int tid, GV gv) 
{
  lv->tid   = tid;
  lv->gv    = gv;
  lv->related_num_items = 0;
}

// Producer thread function
void* do_thread_producers(void *v) 
{
	LV   lv;
	GV   gv;
	lv = (LV) v;
	gv = (GV) lv->gv;

	while(1) 
	{
		int item = 1;
		struct Semaphore_Ring * g_s_buffer = &(gv->sema_buffer);

		// P(mutex)
		pthread_mutex_lock(&(g_s_buffer->mutex_lock));
		// If the ring buffer is not full, P(emptycount)
		while(is_ring_full(g_s_buffer))
		{
			pthread_cond_wait(&(g_s_buffer->emptyCount), &(g_s_buffer->mutex_lock));
		}
		int sleep_seconds = rand_sleep_seconds(g_s_buffer->max_sleep_seconds);

		// If the number of produced items by all threads == the number of items needed to be produced
		if(g_s_buffer->num_produced_items == g_s_buffer->total_number_items2produce)
		{
			printf("Producer thread #%d finishes its work! It has produced %d items.\n",(lv->tid)+1, lv->related_num_items);
			pthread_mutex_unlock(&(g_s_buffer->mutex_lock));
			break;
		}

		// Add 1 item to ring buffer
		add_items_in(g_s_buffer, item);
		// Increase the related number of items (by this thread)
		lv->related_num_items++;
		// Increase the related number of items (by ALL thread)
		g_s_buffer->num_produced_items++;

		// If the ring buffer is not empty, V(fullcount)
		if(is_ring_empty(g_s_buffer) == 0)
		{
			pthread_cond_broadcast(&(g_s_buffer->fullCount));
		}

		// V(mutex)
		pthread_mutex_unlock(&(g_s_buffer->mutex_lock));
		sleep(sleep_seconds);
		
		printf("--> Producer thread #%d produces 1 item\n",(lv->tid)+1);
	}
	// exit the thread and return the number of produced items
	pthread_exit((void *)lv->related_num_items);
}

// Consumer thread function
void* do_thread_consumers(void *v)
{
	LV	lv;
	GV	gv;
	lv = (LV) v;
	gv = (GV) lv->gv;

	while(1)
	{
		struct Semaphore_Ring * g_s_buffer = &(gv->sema_buffer);

		// P(mutex)
		pthread_mutex_lock(&(g_s_buffer->mutex_lock));
		// If the ring buffer is empty and the work is not finished, P(fullcount)
		while(is_ring_empty(g_s_buffer) && g_s_buffer->num_produced_items < g_s_buffer->total_number_items2produce)
		{
			pthread_cond_wait(&(g_s_buffer->fullCount), &(g_s_buffer->mutex_lock));
		}
		int sleep_seconds = rand_sleep_seconds(g_s_buffer->max_sleep_seconds);

		// If consumed all produced items
		if(g_s_buffer->num_consumed_items == g_s_buffer->total_number_items2produce)
		{
			printf("Consumer thread #%d finishes its work! It has consumed %d items.\n",(lv->tid)+1, lv->related_num_items);
			pthread_mutex_unlock(&(g_s_buffer->mutex_lock));
			break;
		}
		int ret_item;
		remove_items(g_s_buffer, &ret_item);

		// Increase the related number of items (by this thread)
		lv->related_num_items++;
		//increase the related number of items (by ALL thread)
		g_s_buffer->num_consumed_items++;

		// V(emptycount)
		if(is_ring_full(g_s_buffer) == 0)
		{
			pthread_cond_broadcast(&(g_s_buffer->emptyCount));
		}

		// V(mutex)
		pthread_mutex_unlock(&(g_s_buffer->mutex_lock));
		sleep(sleep_seconds);
		
		printf("--> Consumer thread #%d consumes 1 item\n",(lv->tid)+1);
	}
	//exit the thread and return the number of consumed items
	pthread_exit((void *)lv->related_num_items);
}

int main(int argc, char* argv[]) 
{
  	int             i;
  	LV              lvs_producer;
  	LV		  	  	lvs_consumer;
  	GV              gv;
  	pthread_t       *producer_threads;
  	pthread_t	    *consumer_threads;
  	pthread_attr_t  *producer_attributes;
  	pthread_attr_t  *consumer_attributes;

  	// check the arguments user inputs
  	if(argc != 6) 
  	{
		fprintf(stderr, "Usage: %s num_producers num_consumers max_sleep_seconds total_num_items2produce ring_buffer_size\n", argv[0]);
    	exit(1);
  	}

  	// Input validation
  	for(i = 1; i < 6;i++)
  	{
  		int validation = atoi(argv[i]);
  		if(validation <= 0)
		{
			fprintf(stderr, "the input should be larger than 0\n");
			exit(1);
		}
  	}

  	int number_producers = atoi(argv[1]);
  	int number_consumers = atoi(argv[2]);
  	int max_sleep_seconds = atoi(argv[3]);
  	int total_number_items2produce = atoi(argv[4]);
  	int ring_buffer_size = atoi(argv[5]);
  
  	// Global variable
  	gv = (GV) malloc(sizeof(*gv));
  	// Initialization for GV
  	initial_sema_ring(&(gv->sema_buffer), ring_buffer_size, max_sleep_seconds, total_number_items2produce);
  	// Local variables (producer and consumer) 
  	lvs_producer = (LV) malloc(sizeof(*lvs_producer)*number_producers);
  	lvs_consumer = (LV) malloc(sizeof(*lvs_consumer)*number_consumers);
  	// Threads (producer and consumer) 
 	producer_threads = (pthread_t*) malloc(sizeof(pthread_t)*number_producers);
  	consumer_threads = (pthread_t*) malloc(sizeof(pthread_t)*number_consumers);
  	// Attributes
  	producer_attributes = (pthread_attr_t*) malloc(sizeof(pthread_attr_t)*number_producers);
  	consumer_attributes = (pthread_attr_t*) malloc(sizeof(pthread_attr_t)*number_consumers);
	
  	// Producer threads creation
  	for(i = 0; i < number_producers; i++) 
  	{
    	init_lv(lvs_producer+i, i, gv);
    	if(pthread_attr_init(producer_attributes+i)) perror("attr_init()");
    	if(pthread_attr_setscope(producer_attributes+i, PTHREAD_SCOPE_SYSTEM)) perror("attr_setscope()");
    	if(pthread_create(producer_threads+i, producer_attributes+i, do_thread_producers, lvs_producer+i)) 
		{
      		perror("pthread_create()");
      		exit(1);
    	}
  	}
  
  	// Consumer threads creation
  	for(i = 0; i < number_consumers; i++) 
  	{
    	init_lv(lvs_consumer+i, i, gv);
    	if(pthread_attr_init(consumer_attributes+i)) perror("attr_init()");
    	if(pthread_attr_setscope(consumer_attributes+i, PTHREAD_SCOPE_SYSTEM)) perror("attr_setscope()");
    	if(pthread_create(consumer_threads+i, consumer_attributes+i, do_thread_consumers, lvs_consumer+i)) 
		{
     		perror("pthread_create()");
      		exit(1);
    	}
  	}
  
  	// Join all producer threads and calculate total items
  	int total_produced_items = 0;
	for(i = 0; i < number_producers; i++) 
	{  	
    	void *number_items;
    	pthread_join(producer_threads[i], &number_items);
    	//printf("Producer thread #%d has produced %d items\n", (i+1), (int)number_items);
    	total_produced_items += (number_items);
  	}
  
 	// Join all consumer threads and calculate total items
  	int total_consumed_items = 0;
  	for(i = 0; i < number_consumers; i++) 
	{
    	void *number_items;
    	pthread_join(consumer_threads[i], &number_items);
    	//printf("Consumer thread #%d has consumed %d items\n", (i+1), (int)number_items);
    	total_consumed_items += (number_items);
  	}
  	
  	// Print statistics
  	printf("Statistics:\n");
  	printf("Total number of items produced : %d\n", total_produced_items);
  	printf("Total number of items consumed : %d\n", total_consumed_items);
    printf("User input: \"Total_Number_Item2Produce\" is %d\n", total_number_items2produce);

	// If #TotalProduced = #TotalConsumed = Total_number_item2Produce, print "Success"
  	if(total_produced_items == total_consumed_items && total_produced_items == total_number_items2produce) 
	{
  		printf("Success!\n");
  	}
  	
  	free(gv);
  	free(lvs_producer);
  	free(lvs_consumer);
  	free(producer_attributes);
  	free(consumer_attributes);
  	free(producer_threads);
  	free(consumer_threads);

  	return 0; 
}
