#pragma once

#include <x86intrin.h>
#include <stdint.h>
#include <vector>

/************************************data type************************************/
typedef struct flush_record_s flush_record_t;
struct flush_record_s
{
    uintptr_t addr;
    uint64_t ts; //_rdtsc()
    flush_record_s(uintptr_t addr=0, uint64_t ts=0):addr(addr), ts(ts){}
};


typedef struct fcnode_s fcnode_t;
struct fcnode_s
{
    fcnode_t * next;
    uintptr_t addr;
};


/************************************variables************************************/
inline uint64_t count_flush = 0;    //总次数
inline uint64_t count_flush_256 = 0;    //跨256总次数
inline std::vector <std::vector<flush_record_t>> flush_record; 
inline bool not_warmup = false;

extern __thread int thread_id;

//xpbuffer 数量
#define BUFFER_NUM 256
#define ALIGN_256(x) (x >> 8ULL) 


inline fcnode_t * fchead, * fctail;
inline std::vector<uintptr_t> flush_addrs; 
inline int threadnum;

/***************************************************************************************/



inline void flush_count_init(int num_threads)
{    
    threadnum = num_threads;
#ifdef DPTREE
    threadnum += 17;
#endif
    fchead = (fcnode_t *) malloc (sizeof(fcnode_t));
    fchead->next = NULL;
    for (int i = 0; i < BUFFER_NUM; i++)
    {
        fcnode_t * tmp = (fcnode_t *) malloc (sizeof(fcnode_t));
        tmp->next = fchead->next;
        tmp->addr = 0;
        fchead->next = tmp;
        if (i == 0) fctail = tmp;
    }
    
    flush_record.resize(threadnum+1);
    flush_addrs.clear();
    count_flush = 0;
    count_flush_256 = 0;
    not_warmup = false;
}

inline void list_insert_tail(fcnode_t * pre, fcnode_t * now)
{
    if (now == fctail) 
    {
        return;
    }
    pre->next = now->next;
    fctail->next = now;
    now->next = NULL;
    fctail = now;
    assert(fchead->next);
}


inline void flush_record_merge()
{
    printf("flush record merge ..\n");

    flush_addrs.clear();
    int pos[100], sz[100];
    memset(pos, 0, sizeof(pos));
    for (int i = 0; i <= threadnum; i++) sz[i] = flush_record[i].size();

    while(true){
        bool flag = false;
        uint64_t minn = 0x7fffffffffffffffULL;
        int which;
        for (int i = 0; i <= threadnum; i++)
        {
            if (pos[i] < sz[i] && flush_record[i][pos[i]].ts < minn)
            {
                which = i;
                minn = flush_record[i][pos[i]].ts;
                flag = true;
            }
        }
//
        if (!flag) break;
        flush_addrs.push_back( flush_record[which][ pos[which]++ ].addr );
    }
    
    printf("flush record merge end.\n");
}

//LRU
inline void flush_count_find_buffer(uintptr_t addr)
{
    fcnode_t * now = fchead->next;
    fcnode_t * pre = fchead;
    while (now)
    {
        if (ALIGN_256(addr) == now->addr)
        {
            //now 插入尾部
            list_insert_tail(pre, now);
            break;
        }
        now = now->next;
        pre = pre->next;
    }    
    if (!now)  //不在任一个buffer里
    {
        __sync_fetch_and_add(&count_flush_256, 1);
        now = fchead->next;
        now->addr = ALIGN_256(addr);
        //插入尾部
        list_insert_tail(fchead, now);
    }
    
}

inline void flush_count_process()
{
    flush_record_merge();
    for (uintptr_t & x: flush_addrs)
    {
        count_flush++;
        flush_count_find_buffer(x);
    }
}



inline void flush_count_addr_record(uintptr_t addr)
{    
    flush_record[thread_id].push_back( flush_record_t(addr, _rdtsc()) );
}