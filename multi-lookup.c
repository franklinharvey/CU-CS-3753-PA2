#include "multi-lookup.h"
#include "util.h"
#include "queue.h"


// FLAGS
pthread_cond_t queueFull, queueEmptyFlag;
pthread_mutex_t queueMutexFlag, producerMutexFlag, consumerMutexFlag;

// GLOBAL VARS
queue sharedBuffer;
int finishedInputFilesCount, inputCount, maxThreads, badInputCount;
char* outputFile;

void* producerPool(char* inputFiles) {
	char** inputFileNames = (char**) inputFiles;

	pthread_t producerThreads[inputCount];
	int i;
	for(i=0; i < inputCount; i++){
		char* fileName = inputFileNames[i];
		pthread_create(&producerThreads[i], NULL, (void*) putFileQueue, (void*) fileName);

		pthread_join(producerThreads[i], NULL);
	}
	for(i=0; i<inputCount; i++){
		// signal requestor threads that the queue is empty so can begin to work.
		pthread_cond_signal(&queueEmptyFlag);
	}
	return NULL;
}

void* consumerPool(FILE* outFile){
	// declare resolver threads.
	pthread_t consumerThreads[inputCount];
	int i;
	// create resolver threads to perform dns lookup until maxThreads have been created.
	for(i=0; i<maxThreads; i++){
		pthread_create(&consumerThreads[i], NULL, dns, outFile);
		// wait until resolver threads have finished.
		pthread_join(consumerThreads[i], NULL);
	}
	return NULL;

}

void* putFileQueue(char* filename){
	char filePath[BUFFER_SIZE] = "";
  strcat(filePath, filename);


	FILE* inputfp = fopen(filePath, "r");

	if(!inputfp){
		// print error to stderror
		perror("Error: unable to open input file.\n");

		pthread_mutex_lock(&producerMutexFlag);
		badInputCount++;
		pthread_mutex_unlock(&producerMutexFlag);
		return NULL;
	}
	char hostname[BUFFER_SIZE];

	while(fscanf(inputfp, INPUTFS, hostname) > 0){
		pthread_mutex_lock(&queueMutexFlag);

		while(queue_is_full(&sharedBuffer)){
			pthread_cond_wait(&queueFull, &queueMutexFlag);
		}
		queue_push(&sharedBuffer, strdup(hostname));
		pthread_cond_signal(&queueEmptyFlag);
		pthread_mutex_unlock(&queueMutexFlag);
	}

	pthread_mutex_lock(&producerMutexFlag);
	finishedInputFilesCount++;
	pthread_mutex_unlock(&producerMutexFlag);
	fclose(inputfp);
	return NULL;
}

void* dns(FILE* outFile) {
	while(1){
		pthread_mutex_lock(&queueMutexFlag);
		while(queue_is_empty(&sharedBuffer)){
			pthread_mutex_lock(&producerMutexFlag);
			int finished = 0;
			if(finishedInputFilesCount + badInputCount == inputCount) finished = 1;
			pthread_mutex_unlock(&producerMutexFlag);
			if (finished){
				pthread_mutex_unlock(&queueMutexFlag);
				return NULL;
			}
			pthread_cond_wait(&queueEmptyFlag, &queueMutexFlag);
		}
		char* hostname = (char*) queue_pop(&sharedBuffer);
		pthread_cond_signal(&queueFull);
		char firstIp[MAX_IP_LENGTH];

		if (dnslookup(hostname, firstIp, sizeof(firstIp))==UTIL_FAILURE){
			fprintf(stderr, "Error: DNS lookup failure on hostname: %s\n", hostname);
			strncpy(firstIp, "", sizeof(firstIp));
		}

		// CLEAN
		pthread_mutex_lock(&consumerMutexFlag);
		fprintf(outFile, "%s,%s\n", hostname, firstIp);
		pthread_mutex_unlock(&consumerMutexFlag);
		free(hostname);
		pthread_mutex_unlock(&queueMutexFlag);
	}
	return NULL;
}

int main(int argc, char* argv[]){
	finishedInputFilesCount = 0;
	inputCount = argc-2;
	outputFile = argv[argc-1];
	char* inputFiles[inputCount];

	maxThreads = MIN_THREADS;

	// INIT
	queue_init(&sharedBuffer, QUEUEMAXSIZE);
	pthread_cond_init(&queueFull, NULL);
	pthread_cond_init(&queueFull, NULL);
	pthread_mutex_init(&queueMutexFlag, NULL);
	pthread_mutex_init(&producerMutexFlag, NULL);
	pthread_mutex_init(&consumerMutexFlag, NULL);

	if (argc < MIN_ARGUMENTS){
		fprintf(stderr, "Not enough arguments: %d", (argc-1));
		return(EXIT_FAILURE);
	}

	// Create array of input files.
	int i;
	for(i=0; i<inputCount; i++){
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
		perror("Error: Create Thread on Produce");
		exit(EXIT_FAILURE);
	}

	// declare resolver ID thread, used to initialize resolver thread pool.
	pthread_t resolverID;

	// resolver = consumer, startup for resolver pool.
	// pthread_create(pthread_t *threadName, const pthread_attr * attr, void*(*start_routine), void* arg)
	int consumer = pthread_create(&resolverID, NULL, (void*) consumerPool, outFile);
	if (consumer != 0){
		errno = consumer;
		perror("Error: Create Thread on Consume");
		exit(EXIT_FAILURE);
	}

  // CLEAN
	pthread_join(requestorID, NULL);
	pthread_join(resolverID, NULL);
	fclose(outFile);
	queue_cleanup(&sharedBuffer);
	pthread_mutex_destroy(&consumerMutexFlag);
	pthread_mutex_destroy(&queueMutexFlag);
	pthread_mutex_destroy(&producerMutexFlag);
	pthread_cond_destroy(&queueEmptyFlag);
	pthread_cond_destroy(&queueFull);
	pthread_exit(NULL);

	return EXIT_SUCCESS;

}
