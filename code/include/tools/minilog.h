#pragma once
typedef struct mini_item_s mini_item_t;
typedef struct minilog_s minilog_t;
#define MINILOG_ITEM_SIZE (sizeof(mini_item_t))
#define MINILOG_SIZE (sizeof(minilog_t))

#define MINILOG_TYPE_SMALL_ALLOC 1
#define MINILOG_TYPE_SMALL_FREE 2
#define MINILOG_TYPE_LARGE_ALLOC 3
#define MINILOG_TYPE_LARGE_FREE 4

#define G (1ULL * 1024ULL * 1024ULL * 1024ULL) //1G for each log file
#define LOG_FILE_SIZE (1 * G)

// #define GLOBAL_MINILOG //if defined, using one global minilog for all threads.在分裂工作中，global log会更好，而在batch 工作中，local log会更好.
#ifndef GLOBAL_MINILOG
extern __thread uint64_t global_index;
#define MINILOG_NUM (LOG_FILE_SIZE / sizeof(mini_item_t) - 10) //NOTE: 要是8的倍数，这样nbanks就不会有问题
#else
extern uint64_t global_index;
#define MINILOG_NUM 512 //NOTE: 要是8的倍数，这样nbanks就不会有问题
#endif
// #define DEBUG_MINILOG

struct mini_item_s
{
   uint64_t key;
   uint64_t value;
   uint64_t timestamp;
};

struct minilog_s
{
   mini_item_t log_item[MINILOG_NUM];
};

#ifndef GLOBAL_MINILOG
minilog_t *minilog_create(int tid); //每个arena有自己的minilog.
#else
minilog_t *minilog_create(); //每个arena有自己的minilog.
#endif

void add_minilog(uint64_t key, uint64_t value);
void add_minilog_for_GC(uint64_t key, uint64_t value);