/*1600011072@pku.edu.cn
 * Aaron Xu
 * version 1.2
 * seg +first fit
 *try something new!
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mm.h"
#include "memlib.h"

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */
#define Debugx
/*
 * If NEXT_FIT defined use next fit search, else use first-fit search
 */
#define NEXT_FITx
#define int_to_ptr(bp)  ((bp)? heap_listp+bp :NULL)
#define ptr_to_int(bp)  ((bp)? ((char*)(bp)-heap_listp):0)
/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  512  /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))
#define MIN(x, y) ((x) < (y)? (x) : (y))
/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))
#define PACKA(size,pre,alloc)  ((size)|(alloc)|(pre<<1))
/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))
#define PUTN(p, val)  (*(unsigned int *)(p) = (val)|GETA(p))
#define PUTA(p)     (*(unsigned int *)(p) = *(unsigned int *)(p)+0x2)
#define PUTF(p)      (*(unsigned int *)(p) = *(unsigned int *)(p)-0x2)
#define GETA(p)     (*(unsigned int *)(p))&0x2
/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)


/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
#define AD_NEXT(bp)    ((char *)(bp))    //return the address of next pointer
#define AD_PREV(bp)    ((char *)bp+WSIZE)  //return the addresso of prev pointer

/* Global variables */
static char *heap_listp = 0;  /* Pointer to first block */

#ifdef NEXT_FIT
static char *rover;           /* Next fit rover */
#endif

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void insert_list(void *bp);
static void delete_list(void *bp);
static char *seg_list[30];
static int seg_root(int asize);
/*Noted that first_node->pre=NULL, last_node->next=NULL*/

/*
 * mm_init - Initialize the memory manager
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(34*WSIZE)) == (void *)-1)
        return -1;
    
    PUT(heap_listp, 0);                          /* Alignment padding */
    for(int i=0;i<30;i++){
        PUT(heap_listp + (i*WSIZE),0);
        seg_list[i]=heap_listp+(i*WSIZE);
    }
    
    PUT(heap_listp + (30*WSIZE),0);
    PUT(heap_listp + (31*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (32*WSIZE), PACK(DSIZE, 1));     /* prologue  header */
    PUT(heap_listp + (33*WSIZE), PACKA(0,1,1));     /* Epilogue header */
    
    heap_listp += (32*WSIZE);
    
    
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
#ifdef Debug
    mm_checkheap(__LINE__);
#endif
    return 0;
}
int seg_root(int asize){
    if(asize<=8) return 0;
    int i,cnt=0;
    for(i=1;i<asize;i=i<<1){
        cnt++;
    }
    cnt-=3;
    return cnt;
}
/*
 * malloc - Allocate a block with at least size bytes of payload
 */
void *malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;
    
    
    
    //printf("ask for %d\n",size);
    if (heap_listp == 0){
        mm_init();
    }
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;
    
    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= WSIZE)
        asize = DSIZE;
    else
        asize = DSIZE * ((size + WSIZE + (DSIZE-1)) / DSIZE);
    
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        
        place(bp, asize);
        return bp;
    }
    
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/DSIZE)) == NULL)
        return NULL;
    
    place(bp, asize);
    
#ifdef Debug
    mm_checkheap(__LINE__);
#endif
    return bp;
}

/*
 * free - Free a block
 */
void free(void *bp)
{
    if (bp == 0)
        return;
    
    size_t size = GET_SIZE(HDRP(bp));
    if (heap_listp == 0){
        mm_init();
    }
    
    PUTN(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    
    PUTF(HDRP(NEXT_BLKP(bp)));
    /*initiate the block before calling coalesce()*/
    PUT(AD_PREV(bp), 0);
    PUT(AD_NEXT(bp), 0);
#ifdef Debug
    mm_checkheap(__LINE__);
#endif
    coalesce(bp);
}

/*
 * realloc - Naive implementation of realloc
 */
void *realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;
    
    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        mm_free(ptr);
        return 0;
    }
    
    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) {
        return malloc(size);
    }
    
    newptr = malloc(size);
    
    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return 0;
    }
    
    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);
    
    /* Free the old block. */
    mm_free(ptr);
    
    return newptr;
}


/*
 * The remaining routines are internal helper routines
 */

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t dwords)
{
    char *bp;
    size_t size;
    
    /* Allocate an even number of words to maintain alignment */
    size = (dwords % 2) ? (dwords+1) * DSIZE : dwords * DSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    /* Initialize free block header/footer and the epilogue header */
    PUTN(HDRP(bp), PACK(size, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACKA(0,0,1)); /* New epilogue header */
    
    
    // printf("ex:%d %d\n",GET_SIZE(HDRP(bp)),size);
    /*initiate the block before calling coalesce()*/
    PUT(AD_PREV(bp), 0);
    PUT(AD_NEXT(bp), 0);
    /* Coalesce if the previous block was free */
    
    return coalesce(bp);
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp)
{
    int a=GETA(HDRP(bp));   //a==1 allocated!
    size_t prev_alloc = (a)? 1:0;
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    /*LIFO Policy*/
    
    if (prev_alloc && next_alloc) {            /* Case 1 */
        
    }
    
    else if (prev_alloc && !next_alloc) {      /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        delete_list(NEXT_BLKP(bp));
        PUTN(HDRP(bp), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0));
        
    }
    
    else if (!prev_alloc && next_alloc) {      /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        delete_list(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size, 0));
        PUTN(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        
        
        bp = PREV_BLKP(bp);
        
    }
    
    else {                                     /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +GET_SIZE(FTRP(NEXT_BLKP(bp)));
        
        delete_list(PREV_BLKP(bp));
        delete_list(NEXT_BLKP(bp));
        PUTN(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    
    insert_list(bp);
#ifdef Debug
    mm_checkheap(__LINE__);
#endif
    
    return bp;
}

/* insert_list - insert a node into the head of the list
 1.A small trick to reduce operations by first_node->pre=NULL
 2.obey "LIFO"    */
inline void insert_list(void *bp){
    int size=GET_SIZE(HDRP(bp));
    
    char *root=seg_list[seg_root(size)];
    int t=GET(root);
    //printf("insert:rootid:%d size:%d\n",seg_root(size),size);
    if(t==0){
        PUT(root, ptr_to_int(bp));
    }
    else{
        PUT(root, ptr_to_int(bp));    //root->next=bp
        PUT(AD_NEXT(bp), t);   //bp->next=t
        PUT(AD_PREV(int_to_ptr(t)), ptr_to_int(bp));   //t->pre=bp;
    }
#ifdef Debug
    mm_checkheap(__LINE__);
#endif
}

/* delete_list - delete a node from the list*/
inline void delete_list(void *bp){
    int size=GET_SIZE(HDRP(bp));
    char *root=seg_list[seg_root(size)];
    //printf("delete:rootid:%d size:%d\n",seg_root(size),size);
    char * prevp=int_to_ptr(GET((AD_PREV(bp))));
    char * nextp=int_to_ptr(GET((AD_NEXT(bp))));
    //if it is the first node
    if(prevp==NULL){
        PUT(root, ptr_to_int(nextp));
        if(nextp!=NULL) PUT(AD_PREV(nextp),0);
    }
    else{
        PUT(AD_NEXT(prevp), ptr_to_int(nextp));
        if(nextp!=NULL) PUT(AD_PREV(nextp), ptr_to_int(prevp));
    }
    
    PUT(AD_PREV(bp), 0);
    PUT(AD_NEXT(bp), 0);
}


/*
 * place - Place block of asize bytes at start of free block bp
 *         and split if remainder would be at least minimum block size
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    delete_list(bp);
    if ((csize - asize) >= (2*DSIZE)) {
        PUTN(HDRP(bp), PACK(asize, 1));  /*a very useful trick: allocate the latter part of the free block, so don't need to change the pointer*/
        bp = NEXT_BLKP(bp);
        
        PUT(HDRP(bp), PACKA(csize-asize,1,0));
        PUT(FTRP(bp), PACK(csize-asize,0));
        PUT(AD_PREV(bp), 0);
        PUT(AD_NEXT(bp), 0);
        coalesce(bp);
        
    }
    else {
        PUTN(HDRP(bp), PACK(csize, 1));
        PUTA(HDRP(NEXT_BLKP(bp)));
    }
#ifdef Debug
    mm_checkheap(__LINE__);
#endif
}

/*
 * find_fit - Find a fit for a block with asize bytes
 */
static void *find_fit(size_t asize)
{
    
    /* First-fit search */
    void *bp;
    int root_id=seg_root(asize);
    char *root;
    while(root_id<30){
        
        root=seg_list[root_id];
        for (bp=int_to_ptr(GET(root)); bp!=NULL && GET_SIZE(HDRP(bp)) > 0; bp = int_to_ptr(GET(AD_NEXT(bp)))) {
            //printf("rootid:%d %d\n",root_id,GET_SIZE(HDRP(bp)));
            if ((asize <= GET_SIZE(HDRP(bp)))) {
                return bp;
            }
        }
        root_id++;//find the next seg list
    }
    return NULL; /* No fit */
    
}

/*
 * mm_checkheap - Check the heap for correctness. Helpful hint: You
 *                can call this function using mm_checkheap(__LINE__);
 *                to identify the line number of the call site.
 */
void mm_checkheap(int lineno)
{
    printf("We are at line %d\n",lineno);
    void *bp;
    char *root=seg_list[0];
    for (bp=int_to_ptr(GET(root)); bp!=NULL && GET_SIZE(HDRP(bp)) > 0; bp = int_to_ptr(GET(AD_NEXT(bp)))) {
        printf("address:%p size:%d,the next is:%p\n",bp,GET_SIZE(HDRP(bp)) , int_to_ptr(GET(AD_NEXT(bp))));
        
    }
    printf("Over-------------------\n");
}
