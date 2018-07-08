#include <stdio.h> // contains printf()
#include <string.h>	// contains strerror()
#include <pthread.h> // thread functions
#include <unistd.h>	// contains sleep()
#include <stdlib.h> // contains atoi()

pthread_t tid[10];
#define NUM_THREADS	10

void *printThread(void *threadid)
{
   long tid;
   tid = (long)threadid;
   printf("Hello World! It's me, thread #%ld!\n", tid);
   pthread_exit(NULL);
}



int main(int argc, char* argv[])
{
	int i = 0;
	int n_threads;
	int err;
	int arg;

	// seed the random number generator
	srand(time(NULL));

	printf ("The parameters are:\n");
	for (arg=0 ; arg < argc ; arg++)
		printf ("\t%3d: [%s]\n", arg, argv[arg]);
		
	n_threads = NUM_THREADS;;
	if (n_threads > 10)
		n_threads = 10;
	
	while (i < n_threads)
	{
		err = pthread_create(&(tid[i]), NULL, &printThread, (void *)i);
	
		if (err != 0)
			printf("\nError thread :[%s]\n", strerror(err));
		else
			printf("\n Thread %d created\n", i);
		i++;
	} 
	
	sleep(5*n_threads);
	printf("\n Main thread exiting\n");
	return 0;
}
