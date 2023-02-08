#include "../util.h"

#ifndef GLOBAL_MINILOG
__thread uint64_t global_index = 0;
#else
minilog_t *minilog_group[thread_id] = (minilog_t *)minilog_create();
uint64_t global_index = 0;
#endif

#ifndef GLOBAL_MINILOG
static inline char *minilog_file_create(uint64_t file_size, size_t align, int pid)
#else
static inline char *minilog_file_create(uint64_t file_size, size_t align)
#endif
{
    char *tmp;

    size_t mapped_len;
    char str[100];
    int is_pmem;
#ifndef GLOBAL_MINILOG
    sprintf(str, "/mnt/pmem/sobtree/minilog_file_%d", pid);
#else
    sprintf(str, "/mnt/pmem/sobtree/minilog_file");
#endif
    //
    if ((tmp = (char *)pmem_map_file(str, file_size, PMEM_FILE_CREATE | PMEM_FILE_SPARSE, 0666, &mapped_len, &is_pmem)) == NULL) //< 待补充错误处理/
    {
        printf("map file fail!1\n %d\n", errno);
        exit(1);
    }
    if (!is_pmem) //< 待补充错误处理/
    {
        printf("is not nvm!\n");
        exit(1);
    }
    // assert((uintptr_t)tmp == roundUp((uintptr_t)tmp, align));

#if 1 //touch
    for (uint64_t i = 0; i < file_size; i += 4096)
    {
        tmp[i] = 1;
    }
    return tmp;
#endif
}

#ifndef GLOBAL_MINILOG
minilog_t *minilog_create(int tid)
#else
minilog_t *minilog_create()
#endif
{

    // //首先创建log_file,大小为8KB，对齐到8KB
    // #ifndef GLOBAL_MINILOG
    //     minilog_t *log = (minilog_t *)((uint64_t)minilog_file_create(1ULL * 1024, tid) + 4 * 1024 - MINILOG_SIZE / 2);
    // #else
    //     minilog_t *log = (minilog_t *)((uint64_t)minilog_file_create(8 * 1024) + 4 * 1024 - MINILOG_SIZE / 2);
    // #endif
    //     memset(log, 0, MINILOG_SIZE);
    //     clflush(log, MINILOG_SIZE);

    //首先创建log_file,大小为8KB，对齐到8KB
#ifndef GLOBAL_MINILOG
    minilog_t *log = (minilog_t *)((uint64_t)minilog_file_create(LOG_FILE_SIZE, 256, tid));
#else
    minilog_t *log = (minilog_t *)((uint64_t)minilog_file_create(LOG_FILE_SIZE, 256));
#endif
    return log;
}

static inline int minilog_count_to_index(int bid)
{
    int alt = bid & 1;
    int bid2 = bid >> 1;                         //loacl里面interleave
    int n = CACHE_LINE_SIZE / MINILOG_ITEM_SIZE; //一个cacheline可以放置多少个元数据
    int nbanks = MINILOG_NUM / 2 / n;            //local拥有这么多个cacheline
    return ((bid2 % nbanks) * n + bid2 / nbanks) + alt * MINILOG_NUM / 2;

    // int n = CACHE_LINE_SIZE / MINILOG_ITEM_SIZE; //一个cacheline可以放置多少个元数据
    // int nbanks = MINILOG_NUM / n;                //拥有这么多个cacheline
    // return (bid % nbanks) * n + bid / nbanks;
}

static inline uint64_t get_slot_in_minilog()
{

    // #ifndef GLOBAL_MINILOG
    //     int index1 = (global_index++) % MINILOG_NUM;
    // #else
    //     int index1 = __sync_fetch_and_add(&global_index, 1) % MINILOG_NUM;
    // #endif
    //     int index2 = minilog_count_to_index(index1); //interleave

#ifndef GLOBAL_MINILOG
    int index1 = (global_index++) % MINILOG_NUM;
#else
    int index1 = __sync_fetch_and_add(&global_index, 1);
#endif

#ifdef DEBUG_MINILOG
    printf("global_index = %d, index1 = %d, index2 = %d , ptr = %p\n", global_index, index1, index2, &minilog_group[thread_id]->log_item[index2]);
#endif
    // return index2;
    return index1;
}

void add_minilog(uint64_t key, uint64_t value)
{
    count_log_group[thread_id]++;
    uint64_t index = get_slot_in_minilog();
    minilog_group[thread_id]->log_item[index].key = key;
    minilog_group[thread_id]->log_item[index].value = value;
    minilog_group[thread_id]->log_item[index].timestamp = _rdtsc();
    clflush(&minilog_group[thread_id]->log_item[index], MINILOG_ITEM_SIZE);
}

void add_minilog_for_GC(uint64_t key, uint64_t value)
{
    count_log_group[thread_id]++;
    uint64_t index = get_slot_in_minilog();
    minilog_group[thread_id]->log_item[index].key = key;
    minilog_group[thread_id]->log_item[index].value = value;
    minilog_group[thread_id]->log_item[index].timestamp = _rdtsc();
    if (index % 2 == 0)
        clflush(&minilog_group[thread_id]->log_item[index], MINILOG_ITEM_SIZE);
    ;
}
