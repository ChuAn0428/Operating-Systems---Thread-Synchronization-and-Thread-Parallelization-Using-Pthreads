#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <math.h>

///////////////////////////////////////////////////////////////////////////
// CSCI 503 Operating Systems
// Lab 3 - Thread Synchronization and Thread Parallelization Using Pthreads
// Part 2: Parallel Matrix Multiplication Problem
// Author: Chu-An Tsai
///////////////////////////////////////////////////////////////////////////

// In this problem, there are two versions of computation for the matrices 
// multiplication. The first one is the sequential version, and it is used 
// to verify the product of two matrices for the second version, multithreaded
// version. The results of the two versions will be compared with one another 
// and see if the computation is correct or not. 

//////////////////////

// Structure definition
struct Semaphore_Indicator_Matrices {
    pthread_mutex_t mutex_lock;
    int ** indicator_matrices;
    int matrices_size;
    int next_i;
    int next_j;
};

typedef struct lv_t {
    int             tid;
    void            *gv;
}* LV;

typedef struct gv_t {
	double ** A;
    double ** B;
    double ** multi_result_matrix;
    struct Semaphore_Indicator_Matrices semaphore_matrices;
}* GV;

// Initialize the semaphore indicator matrix
void init_sem_mat (struct Semaphore_Indicator_Matrices * semaphore_matrices, int matrices_size) 
{
	semaphore_matrices->indicator_matrices = (int **) malloc(sizeof(int *) * matrices_size);
	int i,j;
	// all 0 matrix
	for(i = 0; i < matrices_size; i++)
	{
		(semaphore_matrices->indicator_matrices)[i] = (int *)malloc(sizeof(int) * matrices_size);
		for(j = 0; j < matrices_size;j++)
		{
			(semaphore_matrices->indicator_matrices)[i][j] = 0;
		}
	}
	semaphore_matrices->matrices_size = matrices_size;
	semaphore_matrices->next_i = 0;
	semaphore_matrices->next_j = 0;
	if(pthread_mutex_init(&semaphore_matrices->mutex_lock, NULL)!=0){
		perror("mutex init error!\n");
		exit(1);
	}
}

// Sequential version
double ** sequential_multiplication(double ** A, double ** B, int matrices_size)
{
	int i,j,ii;
	double ** sequential_result_matrix = (double **)malloc(sizeof(double *)* matrices_size);
	for(i=0; i < matrices_size; i++)
	{
		sequential_result_matrix[i] = (double *)malloc(sizeof(double)*matrices_size);
		for(j=0; j < matrices_size; j++)
		{
			sequential_result_matrix[i][j] = 0;
			for(ii=0; ii < matrices_size; ii++)
			{
				sequential_result_matrix[i][j] += A[i][ii] * B[ii][j];
			}
		}
	}
	return sequential_result_matrix;
}

// Generate random double number
double generate_random_double_numbers(double x, double y)
{
	return x + (rand() / (RAND_MAX / (y-x)));
}

// Get the current time for execution time calculation
double get_cur_time() 
{
    struct timeval   tv;
    struct timezone  tz;
    double cur_time;
    gettimeofday(&tv, &tz);
    cur_time = tv.tv_sec + tv.tv_usec / 1000000.0;
    return cur_time;
}

// Initialze the global variable
void init_gv(GV gv, int matrices_size)
{
	double ** A = (double **)malloc(sizeof(double *)*matrices_size);
	double ** B = (double **)malloc(sizeof(double *)*matrices_size);
	double ** multi_result_matrix = (double **)malloc(sizeof(double *)*matrices_size);	
	int i,j;
	// The range of numbers is in [-10, 10)
	for(i=0; i < matrices_size; i++)
	{
		A[i] = (double *) malloc(sizeof(double) * matrices_size);
		B[i] = (double *) malloc(sizeof(double) * matrices_size);
		multi_result_matrix[i] = (double *) malloc(sizeof(double) * matrices_size);
		for(j=0; j < matrices_size; j++)
		{
			A[i][j] = generate_random_double_numbers(-10,10);
			B[i][j] = generate_random_double_numbers(-10,10);
			multi_result_matrix[i][j] = 0;
		}
	}
	gv->A = A;
	gv->B = B;
	gv->multi_result_matrix = multi_result_matrix;
	// Initialize the semephore, including the indicator matrix and mutex lock
	init_sem_mat (&(gv->semaphore_matrices), matrices_size);
}

// Initialize the local variable to each thread
void init_lv(LV lv, int tid, GV gv) 
{
    lv->tid   = tid;
    lv->gv    = gv;
}

// Run each thread
void* do_thread(void *v) 
{
	LV   lv;
	GV   gv;
	int   i;
	lv = (LV) v;
	gv = (GV) lv->gv;

	while(1) 
	{
		// The i and j needed to be computed
		int compute_i = 0;
		int compute_j = 0;
		// Get the semaphore indicator matrix
		struct Semaphore_Indicator_Matrices * g_s_mat = &(gv->semaphore_matrices);
		// lock the mutex
		pthread_mutex_lock(&(g_s_mat->mutex_lock));
		// Critical section starts
		int matrices_size = g_s_mat->matrices_size;
		// Check if the loop needs to be broken and the mutex lock needs to be unlocked
		if(g_s_mat->next_i > (matrices_size-1) || g_s_mat->next_j > (matrices_size-1))
		{
			pthread_mutex_unlock(&(g_s_mat->mutex_lock));
			break;
		}

		// Update the indicator matrix and update the next avail i and j
		(g_s_mat->indicator_matrices)[g_s_mat->next_i][g_s_mat->next_j] = 1;
		// Get the calcualted i and j
		compute_i = g_s_mat->next_i;
		compute_j = g_s_mat->next_j;

		if(g_s_mat->next_j == matrices_size-1)
		{
			g_s_mat->next_i++;
			g_s_mat->next_j = 0;
		}
		else
		{
			g_s_mat->next_j++;
		}
		//release the mutex lock
		pthread_mutex_unlock(&(g_s_mat->mutex_lock));

		//do the dot product 
		double ** A = gv->A;
		double ** B = gv->B;
		for(i = 0; i < g_s_mat->matrices_size; i++)
		{
			(gv->multi_result_matrix)[compute_i][compute_j] += A[compute_i][i] * B[i][compute_j];
		}
	}
	return NULL;
}

int main(int argc, char* argv[]) 
{
    int             i,j;
    LV              lvs;
    GV              gv;
    pthread_t       *threads;
    pthread_attr_t  *attributes;
    // Make the random number be different each time even when 
	// user inputs the same Matrix_size, which I think that
	// it is the real random instead of using only rand() 
    srand(time(NULL));

  	// Check argument number
    if(argc != 3) 
	{
    	fprintf(stderr, "Usage: %s Matrix_size Number_threads\n", argv[0]);
    	exit(1);
  	}

  	// Input validation
    for(i = 1; i < 3;i++)
	{
  		int temp = atoi(argv[i]);
  		if(temp <= 0)
		{
			fprintf(stderr, "the input should be larger than 0\n");
			exit(1);
		}
  	}

  	int matrices_size = atoi(argv[1]);
 	int number_threads = atoi(argv[2]);
  	gv = (GV) malloc(sizeof(*gv));
  	init_gv(gv, matrices_size);
  	lvs = (LV) malloc(sizeof(*lvs)*number_threads);
  	threads = (pthread_t*) malloc(sizeof(pthread_t)*number_threads);
  	attributes = (pthread_attr_t*) malloc(sizeof(pthread_attr_t)*number_threads);
  	
  	// Calculation and print the execution time for Multithreaded version
  	double time_start = get_cur_time();
  	// Create pthreads
  	for(i = 0; i < number_threads; i++) 
	{
    	init_lv(lvs+i, i, gv);
    	if(pthread_attr_init(attributes+i)) perror("attr_init()");
    	if(pthread_attr_setscope(attributes+i, PTHREAD_SCOPE_SYSTEM)) perror("attr_setscope()");
    	if(pthread_create(threads+i, attributes+i, do_thread, lvs+i)) 
		{
      	perror("pthread_create()");
      	exit(1);
    	}
  	}
  	// Wait for the threads to finish
  	for(i = 0; i < number_threads; i++) 
	{
    	pthread_join(threads[i], NULL);
  	}
  	double time_end = get_cur_time();
  	// Print the execution time
  	printf("Multithreaded version finished the calculation (spent %.7f seconds)\n",(time_end - time_start));
 	
  	// Calculation and print the execution time for Sequential version
  	time_start = get_cur_time();
  	double ** sequential_result_matrix = sequential_multiplication(gv->A, gv->B, matrices_size);
  	time_end = get_cur_time();
  	printf("Sequential version finished the calculation (spent %.7f seconds)\n",(time_end - time_start));
  	
/*  	
	// For testing
 	// Print A,B and the two versions of result matrices
  	printf("A:\n");
  	for(i= 0; i<matrices_size; i++)
	{
  		for(j=0;j< matrices_size;j++)
		{
			printf("%.3f ",(gv->A)[i][j]);
		}
		printf("\n");
  	}
  	printf("B:\n");
  	for(i= 0; i<matrices_size; i++)
	{
        for(j=0;j< matrices_size;j++)
		{
            printf("%.3f ",(gv->B)[i][j]);
        }
        printf("\n");
  	}
  	printf("Sequential version:\n");
  	for(i= 0; i<matrices_size; i++)
	{
        for(j=0;j< matrices_size;j++)
		{
            printf("%.3f ",(sequential_result_matrix)[i][j]);
        }
       	printf("\n");
  	}
  	printf("Multithreaded version:\n");
  	for(i= 0; i<matrices_size; i++)
	{
        for(j=0;j< matrices_size;j++)
		{
            printf("%.3f ",(gv->multi_result_matrix)[i][j]);
        }
        printf("\n");
  	}
*/

  	// Calculate the error between two results
  	double error = 0;
  	for(i = 0; i < matrices_size; i++)
	{
  		for(j=0; j < matrices_size; j++)
		{
			error = fabs(error + ((gv->multi_result_matrix)[i][j] - sequential_result_matrix[i][j]));
		}
  	}
  	printf("The error between two results from two versions: %.7f\n",error);
  	
	// If the results from the two versions are the same, print "Success!"
	if(error == 0)
	{
  		printf("Success!\n");
  	}
  	else
  	{
  		printf("Two results are not equal!");
	}

  	free(gv);
  	free(lvs);
  	free(attributes);
  	free(threads);

  	return 0; 
}
