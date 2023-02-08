#include <x86intrin.h>
#include <stdint.h>
#include "flush_count.h"

#define CACHE_LINE_SIZE 64
#define USE_SFENCE
#define USE_CLWB

extern bool not_warmup;

#if 0
// #ifdef FLUSH_COUNT

inline bool not_warmup = false;
inline uint64_t count_flush = 0;    //总次数
inline uint64_t count_flush_256 = 0;    //跨256总次数


//xpbuffer 数量
#define BUFFER_NUM 256
#define ALIGN_256(x) (x >> 8ULL) 

typedef struct fcnode_s fcnode_t;
struct fcnode_s
{
    fcnode_t * next;
    uintptr_t addr;
};
inline fcnode_t * fchead, * fctail;
inline pthread_mutex_t fclock;
inline FILE * file_fc;

inline void flush_count_init()
{    
#ifdef TREE
    char treename[] = "tree";
#elif defined(LBTREE)
    char treename[] = "lbtree";
#elif defined(FPTREE)
    char treename[] = "fptree";
#elif defined(UTREE)
    char treename[] = "utree";
#elif defined(DPTREE)
    char treename[] = "dptree";
#elif defined(FLATSTORE)
    char treename[] = "flatstore";
#elif defined(FASTFAIR)
    char treename[] = "fastfair";
#else
    char treename[] = "null";
#endif

#ifdef ZIP
    char pathname[] = "zip";
#else
    char pathname[] = "uni";
#endif

    char str[100];
    extern uint64_t num_threads;
    sprintf(str, "flush_record_%s/flush_record_%s_%d", pathname, treename, num_threads);
    file_fc = fopen(str, "wb");
    assert(file_fc);
    // fchead = (fcnode_t *) malloc (sizeof(fcnode_t));
    // fchead->next = NULL;
    // for (int i = 0; i < BUFFER_NUM; i++)
    // {
    //     fcnode_t * tmp = (fcnode_t *) malloc (sizeof(fcnode_t));
    //     tmp->next = fchead->next;
    //     tmp->addr = 0;
    //     fchead->next = tmp;
    //     if (i == 0) fctail = tmp;
    // }
    pthread_mutex_init(&fclock, NULL);
}

inline void list_insert_tail(fcnode_t * pre, fcnode_t * now)
{
    // pthread_mutex_lock(&fclock);
    if (now == fctail) 
    {
        // pthread_mutex_unlock(&fclock);
        return;
    }
    pre->next = now->next;
    fctail->next = now;
    now->next = NULL;
    fctail = now;
    assert(fchead->next);
    // pthread_mutex_unlock(&fclock);
}

//LRU
inline void flush_count_find_buffer(uintptr_t addr)
{
    pthread_mutex_lock(&fclock);
    fwrite(&addr, sizeof(uintptr_t), 1, file_fc);
    pthread_mutex_unlock(&fclock);

    __sync_fetch_and_add(&count_flush, 1);

    // pthread_mutex_lock(&fclock);

    // fcnode_t * now = fchead->next;
    // fcnode_t * pre = fchead;
    // while (now)
    // {
    //     if (ALIGN_256(addr) == now->addr)
    //     {
    //         //now 插入尾部
    //         list_insert_tail(pre, now);
    //         break;
    //     }
    //     now = now->next;
    //     pre = pre->next;
    // }    
    // if (!now)  //不在任一个buffer里
    // {
    //     __sync_fetch_and_add(&count_flush_256, 1);
    //     now = fchead->next;
    //     now->addr = ALIGN_256(addr);
    //     //插入尾部
    //     list_insert_tail(fchead, now);
    // }
    
    // pthread_mutex_unlock(&fclock);
}

#endif   //#ifdef FLUSH_COUNT



inline void fence()
{
#ifdef USE_SFENCE
    _mm_sfence();
#else
    _mm_mfence();
#endif
}

inline void clflush(void *addr, int len, bool is_log = false)
{
    // fence();

    for (uint64_t uptr = (uint64_t)addr & ~(CACHE_LINE_SIZE - 1); uptr < (uint64_t)addr + len; uptr += CACHE_LINE_SIZE)
    {

#ifndef eADR_TEST

#ifdef USE_CLWB
        _mm_clwb((void *)uptr);         ////////////////////hpy  记得打开
#else
        _mm_clflushopt((void *)uptr);
        // asm volatile(".byte 0x66; clflush %0"
        //              : "+m"(*(volatile char *)uptr));
#endif
#endif  //not eADR_TEST
    
#ifdef FLUSH_COUNT
        if (not_warmup) flush_count_addr_record(uptr);
#endif 

    }

    fence();
}

inline void clflush_nofence(void *addr, int len, bool is_log = false)
{

    for (uint64_t uptr = (uint64_t)addr & ~(CACHE_LINE_SIZE - 1); uptr < (uint64_t)addr + len; uptr += CACHE_LINE_SIZE)
    {
#ifndef eADR_TEST
        _mm_clwb((void *)uptr);   
#endif  //not eADR_TEST   

#ifdef FLUSH_COUNT
        if (not_warmup) flush_count_addr_record(uptr);
#endif 
    }
}
