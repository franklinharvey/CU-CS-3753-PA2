#include "multi-lookup.h"
#include "util.h"
#include "queue.h"

pthread_cond_t queueFull;		// signal/wait on queue full.
pthread_cond_t queueEmpty;		// signal/wait on queue empty. 

pthread_mutex_t queueLock;		// to protect queue.
pthread_mutex_t producerLock;		// to protect counter in readFiles section.
pthread_mutex_t consumerLock;		// to protext output file in dns section.

queue sharedBuffer;

int finishedInputFilesCount;
int numInputFiles;
char* outputFile;
int maxThreads;

void* producerPool(char* inputFiles) {
	char** inputFileNames = (char**) inputFiles;
	// declare requestor threads. 
	pthread_t producerThreads[numInputFiles];
	int i;
	// create requestor threads until number of input files consumed. 
	for(i=0; i < numInputFiles; i++){
		char* fileName = inputFileNames[i];
		pthread_create(&producerThreads[i], NULL, (void*) readFiles, (void*) fileName);

		pthread_join(producerThreads[i], NULL);
	}
	for(i=0; i<numInputFiles; i++){
		// signal requestor threads that the queue is empty so can begin to work.
		pthread_cond_signal(&queueEmpty);
	}
	return NULL;
}

void* consumerPool(FILE* outFile){
	// declare resolver threads. 
	pthread_t consumerThreads[numInputFiles];
	int i;
	// create resolver threads to perform dns lookup until maxThreads have been created.
	for(i=0; i<maxThreads; i++){
		pthread_create(&consumerThreads[i], NULL, dns, outFile);
		// wait until resolver threads have finished. 
		pthread_join(consumerThreads[i], NULL);
	}
	return NULL;

}

void* readFiles(char* filename){
	char filePath[SBUFSIZE] = "";
  strcat(filePath, filename);
	

	FILE* inputfp = fopen(filePath, "r");

	if(!inputfp){
		// print error to stderror
		perror("Error: unable to open input file.\n");

		pthread_mutex_lock(&producerLock);
		finishedInputFilesCount++;
		pthread_mutex_unlock(&producerLock);
		return NULL;
	}
	char hostname[SBUFSIZE];

	while(fscanf(inputfp, INPUTFS, hostname) > 0){
		pthread_mutex_lock(&queueLock);
    
		while(queue_is_full(&sharedBuffer)){
			pthread_cond_wait(&queueFull, &queueLock);
		}
		queue_push(&sharedBuffer, strdup(hostname));
		pthread_cond_signal(&queueEmpty);
		pthread_mutex_unlock(&queueLock);
	}

	pthread_mutex_lock(&producerLock);
	finishedInputFilesCount++;
	pthread_mutex_unlock(&producerLock);
	fclose(inputfp);
	return NULL;
}

void* dns(FILE* outFile) {
	while(1){
		// lock queue mutex to access.
		pthread_mutex_lock(&queueLock);
		// check to see if queue is empty
		while(queue_is_empty(&sharedBuffer)){
			// ensure that we lock files completed counter variable to access.
			pthread_mutex_lock(&producerLock);
			int finished = 0;
			// determine if we are at the end of the queue. 
			if(finishedInputFilesCount == numInputFiles) finished = 1;
			// unlock counter variable
			pthread_mutex_unlock(&producerLock);
			// we are finished so unlock queue. 
			if (finished){
				// unlock queue mutex
				pthread_mutex_unlock(&queueLock);
				return NULL;
			}
			// if the queue is empty but we still have files to consume that means that consumer threads must wait on condition variable emptyQueue.
			pthread_cond_wait(&queueEmpty, &queueLock);
		}
		// pop 1 hostname from the queue.
		char* hostname = (char*) queue_pop(&sharedBuffer);
		// let producer processes know that there is space in the queue.
		pthread_cond_signal(&queueFull);
		// allocate IP address.
		char firstIp[MAX_IP_LENGTH];
		// if we are unable to perform dns lookup on bogus hostname.
		// Error Handling 1: bogus hostname. 
		if (dnslookup(hostname, firstIp, sizeof(firstIp))==UTIL_FAILURE){
			fprintf(stderr, "Error: DNS lookup failure on hostname: %s\n", hostname);
			strncpy(firstIp, "", sizeof(firstIp));
		}
		// lock the output file using resolver mutex lock.
		pthread_mutex_lock(&consumerLock);
		// add in the hostname with its IP address.
		fprintf(outFile, "%s,%s\n", hostname, firstIp);
		// unlock resolver mutex to allow access to output file. 
		pthread_mutex_unlock(&consumerLock);
		// free space for next hostname. 
		free(hostname);
		// unlock queue access. 
		pthread_mutex_unlock(&queueLock);
	}
	return NULL;
}

int main(int argc, char* argv[]){
	finishedInputFilesCount = 0;
	numInputFiles = argc-2;
	outputFile = argv[argc-1];
	char* inputFiles[numInputFiles];
	
	maxThreads = MIN_RESOLVER_THREADS;
	
	printf("Allocating resolver threads based on num cores available: %d threads\n", maxThreads);	

	// initialize queue as described in queue.h
	queue_init(&sharedBuffer, QUEUEMAXSIZE);
	// initialize condition variables to signal/wait on queue full or empty. 
	pthread_cond_init(&queueFull, NULL);
	pthread_cond_init(&queueFull, NULL);
	// initialize mutex locks.
	pthread_mutex_init(&queueLock, NULL);
	pthread_mutex_init(&producerLock, NULL);
	pthread_mutex_init(&consumerLock, NULL);	

	// Check arguments: as in lookup.c
	if (argc < MINARGS){
		fprintf(stderr, "Not enough arguments: %d", (argc-1));
		fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
		return(EXIT_FAILURE);
	}

	// Create array of input files. 
	int i;
	for(i=0; i<numInputFiles; i++){
		inputFiles[i] = argv[i+1];
	}
	
	//open output file.
	FILE* outFile = fopen(outputFile, "w");
		// Error Handling 2: bogus output file. 
		if(!outFile){
		// print to std error and exit.
		perror("Error: unable to open output file");
		exit(EXIT_FAILURE);
	}

	// declare requestor ID thread, used to initialize requestor thread pool.
	pthread_t requestorID;

	// requestor = producer, startup for requestor pool.
	int producer = pthread_create(&requestorID, NULL, (void*) producerPool, inputFiles);
	if (producer != 0){
		errno = producer;
		perror("Error: pthread_create requestor");
		exit(EXIT_FAILURE);
	}

	// declare resolver ID thread, used to initialize resolver thread pool.
	pthread_t resolverID;
	
	// resolver = consumer, startup for resolver pool.
	// pthread_create(pthread_t *threadName, const pthread_attr * attr, void*(*start_routine), void* arg)
	int consumer = pthread_create(&resolverID, NULL, (void*) consumerPool, outFile);
	if (consumer != 0){
		errno = consumer;
		perror("Error: pthread_create resolver");
		exit(EXIT_FAILURE);
	}

  
	pthread_join(requestorID, NULL);
	pthread_join(resolverID, NULL);

  // CLEAN
	fclose(outFile);
	queue_cleanup(&sharedBuffer);
	pthread_mutex_destroy(&consumerLock);
	pthread_mutex_destroy(&queueLock);
	pthread_mutex_destroy(&producerLock);
	pthread_cond_destroy(&queueEmpty);
	pthread_cond_destroy(&queueFull);
	pthread_exit(NULL);

	return EXIT_SUCCESS;

}

