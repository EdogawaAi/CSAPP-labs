#include "cachelab.h"
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>


typedef struct
{
    int valid;//每个缓存行中v
    int tag;//定义每一个line的tag
    int time_tamp;//定义了时间戳,寻找每一个缓存行来的时间
}cache_line;

typedef struct
{
    int S;//每个缓存行有s位时，S=2^s个
    int E;//每个set中的缓存行数
    int B;//每个缓存行有b位时,B=2^b
    cache_line** line;
}cache;


cache* Cache = NULL;
int miss_count = 0;
int hit_count = 0;
int eviction_count = 0;
int verbose = 0;//是否打印详细信息
char t[100];

void initCache(int s, int E, int b){
    Cache = (cache*) malloc(sizeof(cache));
    Cache->S = 1<<s;
    Cache->E = E;
    Cache->B = 1<<b;
    Cache->line = (cache_line**) malloc(sizeof(cache_line*) * (1 << s));
    for(int i = 0; i < (1<<s); i++){
        Cache->line[i] = (cache_line*) malloc(sizeof(cache_line) * E);
        for(int j = 0; j < E; j++){
            Cache->line[i][j].valid = 0;
            Cache->line[i][j].tag = -1;
            Cache->line[i][j].time_tamp = 0;
        }
    }
}

int getIndex(int op_set, int op_tag){
    for(int i = 0; i < Cache->E; i++){
        if(Cache->line[op_set][i].valid && Cache->line[op_set][i].tag == op_tag){
            return i;
        }
    }
    return -1;//有两种情况：冷不命中,或者所有的缓存行都满了
}

int isFull(int op_set){
    for(int i = 0; i < Cache->E; i++){
        if(Cache->line[op_set][i].valid == 0)
            return i;
    }
    return -1;
}

void update(int i, int op_set, int op_tag){
    Cache->line[op_set][i].valid = 1;
    Cache->line[op_set][i].tag = op_tag;
    for(int k = 0; k < Cache->E; k++){
        Cache->line[op_set][k].time_tamp++;
    }
    Cache->line[op_set][i].time_tamp = 0;
}//每次更新缓存块，其他的时间戳++，被挤兑的缓存块置零。

int findLRU(int op_set){
    int maxIndex = 0;
    int maxStamp = 0;
    for(int i = 0; i < Cache->E; i++){
        if(Cache->line[op_set][i].time_tamp > maxStamp){
            maxIndex = i;
            maxStamp = Cache->line[op_set][i].time_tamp;
        }
    }
    return maxIndex;
}

void updateInfo(int op_set, int op_tag){
    int index = getIndex(op_set, op_tag);
    //是否命中
    if(index == -1){
        miss_count++;
        if(verbose){
            printf("miss ");
        }
        int i = isFull(op_set);
        if(i == -1){
            eviction_count++;
            if(verbose){
                printf("eviction\n");
            }
            i = findLRU(op_set);
        }
        update(i, op_set, op_tag);
    }else {
        hit_count++;
        if(verbose){
            printf("hit\n");
        }
        update(index, op_set, op_tag);
    }
}

void freeCache(){
    for(int i = 0; i < Cache->E; i++){
        free(Cache->line[i]);
    }
    free(Cache->line);
    free(Cache);
}

void getTrace(int s, int E, int b){
    FILE* fp;
    fp = fopen(t, "r");
    if(fp == NULL){
        exit(-1);
    }
    char identifier;
    unsigned address;
    int size;
    //读取M 20,1/L 19,3
    while(fscanf(fp, " %c %x %d", &identifier, &address, &size) > 0){
        int op_tag = address >> (s + b);
        int op_set = (address >> b) & ((1 << s) - 1);
        switch (identifier)
        {
        case 'M':
            updateInfo(op_set, op_tag);
            updateInfo(op_set, op_tag);
            break;
        case 'L':
            updateInfo(op_set, op_tag);
            break;
        case 'S':
            updateInfo(op_set, op_tag);
            break;
        }
    }
    fclose(fp);
}


int main(int argc, char* argv[])
{
    //printSummary(0, 0, 0);
    char opt;
    int s, E, b;
    while (-1 != (opt = getopt(argc, argv, "hvs:E:b:t:")))
    {
        switch (opt)
        {
        case 'h':
            printf("input format wrong");
            break;
        case 'v':
            verbose = 1;
            break;
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            strcpy(t, optarg);
            break;
        default:
            printf("input format wrong");
            break;
        }
    }
    initCache(s, E, b);
    getTrace(s, E, b);
    freeCache();
    printSummary(hit_count, miss_count, eviction_count);

    return 0;
}
