//
// Created by hoshino-cola on 24-8-19.
//

#ifndef CACHE_H
#define CACHE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_CACHELINE_NUM 10

typedef struct
{
    char url[MAXLINE];
    char obj[MAX_OBJECT_SIZE];

    int read_cnt;
    time_t timestamp;
    sem_t mutex_reader;
    sem_t mutex_write;
}CacheLine;

typedef struct
{
    CacheLine data[MAX_CACHELINE_NUM];
}Cache;

Cache* init_cache();

int reader(Cache *cache, int fd, char *url);

static int remove_index(Cache *cache);

void writer(Cache *cache, char *url, char *buf);

void delete_cache(Cache *cache);

#endif //CACHE_H
