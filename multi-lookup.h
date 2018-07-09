#ifndef MULTI_LOOKUP_H
#define MULTI_LOOKUP_H

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "util.h"
#include "queue.h"

#define MIN_ARGUMENTS 3
#define BUFFER_SIZE 1025
#define INPUTFS "%1024s"
#define MAX_IP_LENGTH INET6_ADDRSTRLEN
#define MIN_THREADS 2

void* putFileQueue(char* filename);

void* producerPool(char* inputFiles);

void* dns();

void* consumerPool();

int main(int argc, char* argv[]);

#endif
