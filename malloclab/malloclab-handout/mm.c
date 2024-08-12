/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "SJTU",
    /* First member's full name */
    "hoshino-cola",
    /* First member's email address */
    "Hoshino.Cola@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 头部/脚部的大小 */
#define WSIZE 4
/* 双字 */
#define DSIZE 8

/* 扩展堆时的默认大小 */
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y)? (x) : (y))
#define MIN(x, y) ((x) < (y)? (x) : (y))

/* 设置头部和脚部的值, 块大小+分配位 */
#define PACK(size, alloc) ((size) | (alloc))

/* 读写指针p的位置 */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val) | GET_TAG(p))
#define PUT_NOTAG(p, val) ((*(unsigned int *)(p)) = (val))

#define SET_PTR(p, ptr) (*(unsigned int *)(p) = (unsigned int)(ptr))

/* 从头部或脚部获取大小或分配位 */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_TAG(p) (GET(p) & 0x2)
#define SET_RTAG(p) (GET(p) |= 0x2)
#define REMOVE_RTAG(p) (GET(p) &= ~0x2)


//给定bp，找到块的头部
// +-----------------+ <- HDRP(bp)
// | Header (4 bytes)|
// +-----------------+ <- bp
// | Payload (24 bytes)|
// | (user data)     |
// +-----------------+ <- FTRP(bp)
// | Footer (4 bytes)|
// +-----------------+
// /* 给定序号，找到链表头节点位置 */

/* 给定有效载荷指针, 找到头部和脚部 */
#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* 给定有效载荷指针, 找到前一块或下一块 */
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

//Address of free block's predecessor and successor entries 
#define PRED_PTR(bp) ((char *)(bp))
#define SUCC_PTR(bp) ((char *)(bp) + WSIZE)

// Address of free block's predecessor and successor on the segregated list 
#define PRED(ptr) (*(char **)(ptr))
#define SUCC(ptr) (*(char **)(SUCC_PTR(ptr)))


#define CLASS_SIZE 20
#define REALLOC_BUFFER (1 << 7)

/* 总是指向序言块的第二块 */
static char *heap_list;
static void *segregated_free_lists[CLASS_SIZE];


/************************/
static void *extend_heap(size_t words);     //扩展堆
static void *coalesce(void *bp);            //合并空闲块
static void *find_fit(size_t asize);        //找到匹配的块
static void *place(void *bp, size_t asize);  //分割空闲块
static int search(size_t size);             //根据块大小, 找到头节点位置
static void insert_node(void *ptr, size_t size);
static void delete_node(void *ptr);

/*
 * 扩展heap, 传入的是字节数
*/
void *extend_heap(size_t size)
{
    /* bp总是指向有效载荷 */
    void *bp;
    size_t asize;
    /* 根据传入字节数奇偶, 考虑对齐 */
    asize = ALIGN(size);

    /* 分配 */
    if((bp = mem_sbrk(asize)) ==(void *) -1)
        return NULL;
    
    /* 设置头部和脚部 */
    PUT_NOTAG(HDRP(bp), PACK(asize, 0));           /* 空闲块头 */
    PUT_NOTAG(FTRP(bp), PACK(asize, 0));           /* 空闲块脚 */
    PUT_NOTAG(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   /* 片的新结尾块 */

    insert_node(bp, asize);

    /* 判断相邻块是否是空闲块, 进行合并 */
    return coalesce(bp);
}



void insert_node(void *ptr, size_t size)
{
    int list = 0;
    void *search_ptr = ptr;
    void *insert_ptr = NULL;

    //select segregated list
    while((list < CLASS_SIZE - 1) && (size > 1)){
        size >>= 1;
        list++;
    }

    search_ptr = segregated_free_lists[list];
    while((search_ptr != NULL) && (size > GET_SIZE(HDRP(search_ptr))))
    {
        insert_ptr = search_ptr;
        search_ptr = PRED(search_ptr);
    }

    //set predecessor and successor
    if (search_ptr != NULL) {
        if (insert_ptr != NULL) {
            SET_PTR(PRED_PTR(ptr), search_ptr);
            SET_PTR(SUCC_PTR(search_ptr), ptr);
            SET_PTR(SUCC_PTR(ptr), insert_ptr);
            SET_PTR(PRED_PTR(insert_ptr), ptr);
        } else {
            SET_PTR(PRED_PTR(ptr), search_ptr);
            SET_PTR(SUCC_PTR(search_ptr), ptr);
            SET_PTR(SUCC_PTR(ptr), NULL);
            segregated_free_lists[list] = ptr;
        }
    } else {
        if (insert_ptr != NULL) {
            SET_PTR(PRED_PTR(ptr), NULL);
            SET_PTR(SUCC_PTR(ptr), insert_ptr);
            SET_PTR(PRED_PTR(insert_ptr), ptr);
        } else {
            SET_PTR(PRED_PTR(ptr), NULL);
            SET_PTR(SUCC_PTR(ptr), NULL);
            segregated_free_lists[list] = ptr;
        }
    }

    return;
}


void delete_node(void *ptr)
{
    int list = 0;
    size_t size = GET_SIZE(HDRP(ptr));

    while((list < CLASS_SIZE - 1) && (size > 1)){
        size >>= 1;
        list++;
    }

    if(PRED(ptr) != NULL){
        if(SUCC(ptr) != NULL){
            SET_PTR(SUCC_PTR(PRED(ptr)), SUCC(ptr));
            SET_PTR(PRED_PTR(SUCC(ptr)), PRED(ptr));
        }
        else{
            SET_PTR(SUCC_PTR(PRED(ptr)), NULL);
            segregated_free_lists[list] = PRED(ptr);
        }
    }
    else{
        if(SUCC(ptr) != NULL){
            SET_PTR(PRED_PTR(SUCC(ptr)), NULL);
        } else{
            segregated_free_lists[list] = NULL;
        }
    }

    return;
}


/*
 * 合并空闲块
*/
void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));     /* 前一块大小 */
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));     /* 后一块大小 */
    size_t size = GET_SIZE(HDRP(bp));                       /* 当前块大小 */

     if(GET_TAG(HDRP(PREV_BLKP(bp))))
        prev_alloc = 1;


    // 前一块已经被分配
    if(prev_alloc && next_alloc){
        return bp;
    }
    /* 前不空后空 */
    else if(prev_alloc && !next_alloc){
        delete_node(bp);
        delete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));  //增加当前块大小
        PUT(HDRP(bp), PACK(size, 0));           //先修改头
        PUT(FTRP(bp), PACK(size, 0));           //根据头部中的大小来定位尾部
    }
    /* 前空后不空 */
    else if(!prev_alloc && next_alloc){
        delete_node(bp);
        delete_node(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));  //增加当前块大小
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);                     //注意指针要变
    }
    /* 都空 */
    else{
        delete_node(bp);
        delete_node(PREV_BLKP(bp));
        delete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));  //增加当前块大小
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    insert_node(bp, size);

    return bp;
}


/*
 * 分离空闲块
 */
void *place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    delete_node(bp);
    
    if((csize - asize) >= 2*DSIZE) {
        if(asize >= 100){
            PUT(HDRP(bp), PACK(csize - asize, 0));
            PUT(FTRP(bp), PACK(csize - asize, 0));
            /* bp指向空闲块 */
            PUT_NOTAG(HDRP(NEXT_BLKP(bp)), PACK(asize, 1));
            PUT_NOTAG(FTRP(NEXT_BLKP(bp)), PACK(asize, 1));
            insert_node(bp, csize - asize);
            //return NEXT_BLKP(bp);
            return NEXT_BLKP(bp);
        }
        else {
            PUT(HDRP(bp), PACK(asize, 1));
            PUT(FTRP(bp), PACK(asize, 1));
            /* bp指向空闲块 */
            PUT_NOTAG(HDRP(NEXT_BLKP(bp)), PACK(csize - asize, 0));
            PUT_NOTAG(FTRP(NEXT_BLKP(bp)), PACK(csize - asize, 0));
            insert_node(NEXT_BLKP(bp), csize - asize);
        }
    }
    /* 设置为填充 */
    else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
    return bp;
}


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* 申请四个字节空间 */
    int list;

    for(list = 0; list < CLASS_SIZE; list++)
        segregated_free_lists[list] = NULL;

    if((heap_list = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    //清空填充块
    PUT(heap_list, 0);
    /* 
     * 序言块和结尾块均设置为已分配, 方便考虑边界情况
     */
    PUT(heap_list + (1*WSIZE), PACK(DSIZE, 1));     /* 填充序言块 */
    PUT(heap_list + (2*WSIZE), PACK(DSIZE, 1));     /* 填充序言块 */
    PUT(heap_list + (3*WSIZE), PACK(0, 1));         /* 结尾块 */

    /* 扩展空闲空间 */
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // int newsize = ALIGN(size + SIZE_T_SIZE); //对齐后的大小
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
	//     return NULL;
    // else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }
    size_t asize;
    size_t extendsize;
    void *bp = NULL;
    if(size == 0)
        return NULL;
    if(size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = ALIGN(size + DSIZE);
    /* 寻找合适的空闲块 */
    int list = 0;
    size_t searchsize = asize;
    while(list < CLASS_SIZE){
        if((list == CLASS_SIZE - 1) || ((searchsize <= 1) && (segregated_free_lists[list] != NULL))){
            bp = segregated_free_lists[list];
            while ((bp != NULL) && ((asize > GET_SIZE(HDRP(bp))) || (GET_TAG(HDRP(bp)))))
            {
                bp = PRED(bp);
            }
            if(bp != NULL)
            {
                break;
            }
        }
        searchsize >>= 1;
        list++;
    }

    if(bp == NULL){
        extendsize = MAX(asize, CHUNKSIZE);


        if((bp = extend_heap(extendsize)) == NULL)
            return NULL;
    }

    /* 找不到则扩展堆 */
    bp = place(bp, asize);
    return bp;

}

/*
 * 适配
 */
void *find_fit(size_t asize)
{
    void *bp;
    for(bp = heap_list; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        if((GET_SIZE(HDRP(bp)) >= asize) && (!GET_ALLOC(HDRP(bp)))){
            return bp;
        }
    }
    return NULL;
}


/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    if(ptr==0)
        return;
    size_t size = GET_SIZE(HDRP(ptr));

    REMOVE_RTAG(HDRP(NEXT_BLKP(ptr)));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    insert_node(ptr, size);
    coalesce(ptr);

    return;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    /* size可能为0,则mm_malloc返回NULL */
    // void *oldptr = ptr;
    // void *newptr;
    // size_t copysize;
    
    // if((newptr = mm_malloc(size))==NULL)
    //     return 0;

    // copysize = GET_SIZE(HDRP(oldptr));
    // if(size < copysize)
    //     copysize = size;
    // memcpy(newptr, oldptr, copysize);
    // mm_free(oldptr);
    // return newptr;
    void *new_ptr = ptr;
    size_t new_size = size;
    int remainder;
    int extendsize;
    int block_buffer;

    if(size == 0)
        return NULL;

    //Align blobk size
    if(new_size <= DSIZE){
        new_size = 2 * DSIZE;
    }
    else{
        new_size = ALIGN(size + DSIZE);
    }


    new_size += REALLOC_BUFFER;

    block_buffer = GET_SIZE(HDRP(ptr)) - new_size;

    if(block_buffer < 0){
        if(!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) || !GET_SIZE(HDRP(NEXT_BLKP(ptr))))
        {
            remainder = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - new_size;
            if(remainder < 0){
                extendsize = MAX(-remainder, CHUNKSIZE);
                if(extend_heap(extendsize) == NULL)
                    return NULL;
                remainder += extendsize;
            }

            delete_node(NEXT_BLKP(ptr));

            PUT_NOTAG(HDRP(ptr), PACK(new_size + remainder, 1));
            PUT_NOTAG(FTRP(ptr), PACK(new_size + remainder, 1));
        }else{
            new_ptr = mm_malloc(new_size - DSIZE);
            memcpy(new_ptr, ptr, MIN(size, new_size));
            mm_free(ptr);
        }
        block_buffer = GET_SIZE(HDRP(new_ptr)) - new_size;
    }

    if(block_buffer < 2 * REALLOC_BUFFER)
        SET_RTAG(HDRP(NEXT_BLKP(new_ptr)));

    return new_ptr;
}
