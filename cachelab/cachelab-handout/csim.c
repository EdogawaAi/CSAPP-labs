#include "cachelab.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct line
{
    /* data */
    int valid;
    int tag;
    int stamp;
}line;

typedef struct cache{
    int S;
    int E;
    int B;
    line **set;
}cache;

cache* CACHE = NULL;
int miss_count = 0;
int hit_count = 0;
int eviction_count = 0;
int verbose = 0;//是否打印详细信息
char t[100];

void init_cache(int s, int E, int b){
    CACHE = (cache*) malloc(sizeof(cache));
    CACHE ->S = 1 << s;
    CACHE ->B = 1 << b;
    CACHE ->E = E;
    CACHE ->set = (line**) malloc(sizeof(line*) * (1 << s));
    for(int i = 0; i < (1 << s); i++){
        CACHE ->set[i] = (line*)malloc(sizeof(line) * E);
        for(int j = 0; j <E; j++){
            CACHE ->set[i][j].valid = 0;
            CACHE ->set[i][j].tag = -1;
            CACHE ->set[i][j].stamp = 0;
        }
    }

}

//**实现LRU时间戳：最近最少使用策略
int getIndex(int op_set, int op_tag){
    for(int i = 0; i < CACHE ->E; i++ ){
        if(CACHE ->set[op_set][i].valid && CACHE ->set[op_set][i].tag == op_tag){
            return i;
        }
    }
    return -1;
}//找到指定op_set中,tag

int isFull(int op_set){
    for(int i = 0; i < CACHE->E; i++){
        if(CACHE ->set[op_set][i].valid == 0){
            return i;
        }
    }
    return -1;
}

void update(int i, int op_s, int op_tag){
    CACHE ->set[op_s][i].valid = 1;
    CACHE ->set[op_s][i].tag = op_tag;
    for(int k = 0; k < CACHE ->E; k++){
        if(CACHE ->set[op_s][k].valid == 1){
            CACHE ->set[op_s][k].stamp++;
        }
    }
    CACHE ->set[op_s][i].stamp = 0;
}

int find_LRU(int op_s){
    int maxIndex = 0;
    int maxStamp = 0;
    for(int i = 0; i < CACHE ->E; i++){
        if(CACHE ->set[op_s][i].stamp > maxStamp){
            maxStamp = CACHE ->set[op_s][i].stamp;
            maxIndex = i;
        }
    }
    return maxIndex;
}

void updateInfo(int op_set, int op_tag){
    int index = getIndex(op_set, op_tag);
    //未命中
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
            i = find_LRU(op_set);
        }
        update(i, op_set, op_tag);
    }else{
        hit_count++;
        if(verbose){
            printf("hit\n");
        }
        update(index, op_set, op_tag);
    }
}
//
void freeCache(){
    for(int i = 0; i < CACHE->S; i++){
        free(CACHE->set[i]);
    }
    free(CACHE->set);
    free(CACHE);
    CACHE = NULL;
}

void getTrace(int s, int E, int b){
    FILE* pFile;
    pFile = fopen(t, "r");
    if(pFile == NULL){
        exit(-1);
    }
    char identifier;
    unsigned address;
    int size;
    while(fscanf(pFile, "%c %x %d", &identifier, &address, &size) > 0){
        int op_tag = address >> (s + b);
        int op_s = (address >> b) & ((unsigned)(-1) >> (8 * sizeof(unsigned) - s));
        switch (identifier)
        {
        case 'M':
            updateInfo(op_s, op_tag);
            updateInfo(op_s, op_tag);
            break;
        case 'L':
            updateInfo(op_s, op_tag);
            break;
        case 's':
            updateInfo(op_s, op_tag);
            break;
        }
    }
    fclose(pFile);

}

int main(int argc, char* argv[])
{
    //printSummary(0, 0, 0);
    char opt;
    int s, E, b;
    while(-1 != (opt = getopt(argc, argv, "hvs:E:b:t:")))
    {
        switch (opt)
        {
        case 'h':
            printf("input format wrong");
            exit(0);
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
            printf("input format wrong!");
            exit(-1);
        }
    }
    init_cache(s, E, b);
    getTrace(s, E, b);
    freeCache();
    printSummary(hit_count, miss_count, eviction_count);

    return 0;
}
