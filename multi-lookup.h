#ifndef MULTI_LOOKUP_H
#define MULTI_LOOKUP_H

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "util.h"
#include "queue.h"

#define MINARGS 3
#define USAGE "<inputFilePath> <outputFilePath>"
#define SBUFSIZE 1025
#define INPUTFS "%1024s"
// additional assignment specific definitions.
// did not utilize: MAX_THREADS, MAX_INPUT_FILES, chose not to cap those definitions.
#define MAX_IP_LENGTH INET6_ADDRSTRLEN
#define MIN_RESOLVER_THREADS 2

void* readFiles(char* filename);

void* producerPool(char* inputFiles);

void* dns();

void* consumerPool();

int main(int argc, char* argv[]);

#endif
