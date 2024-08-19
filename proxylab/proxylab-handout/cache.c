//
// Created by hoshino-cola on 24-8-19.
//

#include "cache.h"
#include "csapp.h"

Cache* init_cache()
{
    Cache *cache = (Cache*) malloc(sizeof(Cache));
    for(int i = 0; i < MAX_CACHELINE_NUM; i++)
    {
        Sem_init(&cache->data[i].mutex_reader, 0, 1);
        Sem_init(&cache->data[i].mutex_write, 0, 1);
        cache->data[i].read_cnt = 0;
        cache->data[i].timestamp = 0;
    }
    return cache;
}

int reader(Cache *cache, int fd, char *url)
{
    int hit_flag = 0;

    //遍历整个缓存,看是否命中
    for(int i = 0; i < MAX_CACHELINE_NUM; i++)
    {
        if(!strcmp(cache->data[i].url, url))
        {
            //增加一个读者
            P(&cache->data[i].mutex_reader);
            cache->data[i].read_cnt++;
            cache->data[i].timestamp = time(NULL);
            if(cache->data[i].read_cnt == 1)
                P(&cache->data[i].mutex_write);
            V(&cache->data[i].mutex_reader);

            Rio_writen(fd, cache->data[i].obj, MAX_OBJECT_SIZE);
            hit_flag = 1;

            //减少一个读者
            P(&cache->data[i].mutex_reader);
            cache->data[i].read_cnt--;
            if(cache->data[i].read_cnt == 0)
            {
                V(&cache->data[i].mutex_write);
            }
            V(&cache->data[i].mutex_reader);
            break;
        }
    }
    return hit_flag;
}

static int remove_index(Cache *cache)
{
    time_t min = __LONG_MAX__;
    int result = -1;
    for(int i = 0; i < MAX_CACHELINE_NUM; i++)
    {
        if(cache->data[i].timestamp < min)
        {
            min = cache->data[i].timestamp;
            result = i;
        }
    }
    return result;
}

void writer(Cache *cache, char *url, char *buf)
{
    int i = remove_index(cache);
    P(&cache->data[i].mutex_write);
    strcpy(cache->data[i].url, url);
    strcpy(cache->data[i].obj, buf);
    cache->data[i].timestamp = time(NULL);
    V(&cache->data[i].mutex_write);
}

void delete_cache(Cache *cache)
{
    free(cache);
}