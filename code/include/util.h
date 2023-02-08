#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <libpmemobj.h>
#include <libpmem.h>
#include <sys/stat.h>
#include <vector>
#include <set>
#include <queue>
#include <future>
#include <assert.h>
#include <random>
#include <utility>
#include <stdint.h>
#include <pthread.h>
#include <climits>

// #include "tools/atomic_queue/include/atomic_queue/atomic_queue.h"
#include "tools/persist.h"
#include "tools/port_posix.h"
#include "tools/timer.h"
// #include "tools/zipfian_util.h"
// #include "tools/zipfian.h"
#include "tools/utils.h"
#include "tools/zipfian_generator.h"
#include "tools/scrambled_zipfian_generator.h"
#include "tools/minilog.h"
#include "tools/log.h"
#include "tools/nodepref.h"
#include <unistd.h>
#include <sstream>

#include "tools/flush_count.h"

#include "tools/indirect_ptr.h"

#define PRINTF_DEBUG 0
#if PRINTF_DEBUG
#define PRINTF(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#define PRINTF(fmt, ...)
#endif

// #define MIXED_WORKLOAD
#ifndef STRING_TEST
#define key_type_sob int64_t
#define value_type_sob int64_t
#else
#define key_type_sob indirect_ptr
#define value_type_sob indirect_ptr
#endif

// #define mscan_size 100
// #define mmax_length_for_scan (mscan_size + 64)
#define SHUFFLE_KEYS

/**********************************宏开关**************************************/
//【目前没有用 不要打开】
// #define ZIPFIAN

// 打开使用naive GC，关闭则使用不 naive GC
// #define GC_NAIVE

// #define GC_NAIVE_2

//打开是统一结点大小：14个KV.
// #define UNIFIED_NODE

//重复key是否要插入。打开是true。【默认打开】
#define INSERT_REPEAT_KEY

// dptree的背景线程数是否固定（16）。打开是固定，关闭是等于前台线程数。【默认打开】
inline int parallel_merge_worker_num = 16;
// #define FIXED_BACKGROUND

//是否测试peak log memory。打开会在每次gc结束后遍历vlog进行统计。
// #define PERF_LOG_MEMORY

//是否统计flush的次数。打开后会用LRU链表模拟XPBuffer
// #define FLUSH_COUNT

// eADR测试。打开后会取消clwb
//  #define eADR_TEST

// dram空间测试。打开后每次分配非叶子节点时会进行统计
//  #define DRAM_SPACE_TEST

// #define LATENCY_TEST

// #define PERF_GC

/***************************只对lbtree有用**********************************/
// buffer layer测试。打开后lbtree去掉entry moving
//  #define TREE_NO_BUFFER

/***************************以下几个只对tree有用**********************************/
// gc开销测试。打开后tree不做gc。【默认关闭】
//  #define TREE_NO_GC

// selective logging测试。打开后即使当次操作cache已满也会写log
//  #define TREE_NO_SELECLOG

// search cache测试。打开后search只在叶子中进行，不在leaf进行
//  #define TREE_NO_SEARCHCACHE

/***************************以下6个只对normal test有用**********************************/
// warm up【默认打开】
#define DO_WARMUP

// insert【默认打开】
#define DO_INSERT

// #define DO_UPDATE

// #define DO_SEARCH

// #define DO_SCAN

// #define DO_DELETE

/**********************************只对lbtree tree有用*********************************/
// #define DO_RECOVERY

/*****************************************************global variable**********************************/

inline __thread int thread_id;
inline uint64_t num_keys;
inline uint64_t num_threads;

inline HistogramSet *hist_set_group[100];
inline minilog_t *minilog_group[100];
inline uint64_t elapsed_time_group[100];
inline uint64_t count_log_group[100];
inline uint64_t pre_total_log = 0;
inline uint64_t count_lnode_group[100];

inline uint64_t count_search_in_dram[100];

inline uint64_t count_error_insert[100];
inline uint64_t count_error_update[100];
inline uint64_t count_error_search[100];
inline uint64_t count_error_delete[100];
inline uint64_t count_error_scan[100];

inline uint64_t count_conflict_in_bnode[100];

inline uint64_t free_bnode[100];
inline uint64_t create_bnode[100];

//每个线程的require请求数量，用于热点检测
inline uint64_t count_access[2][100];

// dram空间
inline uint64_t dram_space;

#ifdef LATENCY_TEST
inline uint64_t latency_count[10000001];
inline uint64_t ops_count;
#endif
// #define mscan_size 100
// #define mmax_length_for_scan (mscan_size + 64)
inline int mscan_size = 100;
inline int mmax_length_for_scan = 100 + 64;

#ifdef PERF_GC

//采样次数
#define GC_RECORD_CNT 1000
//间隔（us)
#define GC_RECORD_INTERVAL 100000

// global
inline volatile uint64_t global_time_record; //采样次数
inline bool is_gc[GC_RECORD_CNT + 1];		 // is_gc[i]  这次采样时是否在做gc
inline int gc_record_idx;					 // gc record的下标
inline uint64_t gc_record[100];				 // gc时的global_time_record

// for thread
inline __thread uint64_t thread_time_record;
inline int64_t thread_ops_count[50][GC_RECORD_CNT + 1]; //线程数 取样次数

#endif

/*****************************************************global variable**********************************/
#define MEMPOOL //为了避免PMDK会在头部添加16字节元数据的问题，采用自定义的mempool
#ifdef MEMPOOL
#include "tools/mempool.h"

#else
inline PMEMobjpool *pop;
#endif

/*****************************************************global variable**********************************/
inline int file_exists(const char *filename)
{
	struct stat buffer;
	return stat(filename, &buffer);
}

inline void openPmemobjPool(char *pathname, uint64_t size, int num_theads)
{

	int sds_write_value = 0;
	pmemobj_ctl_set(NULL, "sds.at_create", &sds_write_value);
#ifndef MEMPOOL
	if (file_exists(pathname) != 0)
	{
		printf("create new one.\n");
		if ((pop = pmemobj_create(pathname, POBJ_LAYOUT_NAME(btree), size, 0666)) == NULL)
		{
			perror("failed to create pool.\n");
			return;
		}
	}
	else
	{
		printf("open existing one.\n");
		if ((pop = pmemobj_open(pathname, POBJ_LAYOUT_NAME(btree))) == NULL)
		{
			perror("failed to open pool.\n");
			return;
		}
	}
#else

#ifdef DPTREE //有背景线程的话，背景线程也需要分池子

#ifndef FIXED_BACKGROUND
	the_thread_nvmpools.init(num_theads + 1 + num_theads, pathname, size); // dptree有背景线程回收，背景线程要与前台分开。
#else
	printf("parallel_merge_worker_num = %d\n", parallel_merge_worker_num);
	the_thread_nvmpools.init(num_theads + 1 + parallel_merge_worker_num, pathname, size); // dptree有背景线程回收，背景线程要与前台分开。
#endif
#else
	the_thread_nvmpools.init(num_theads + 1, pathname, size); // main thread and background thread(our tree) occupied the last pool.
#endif
#endif
}

// #define PREFETCH
// #define SPLIT_EVALUATION
// #define BACKGROUND
// #define SPLIT_BREAKDOWN

#define ABORT_INODE 5
#define ABORT_BNODE 6
#define ABORT_LNODE 7

/******************************for  log*******************************************/
#if 1
#ifdef PERF_LOG_MEMORY
inline uint64_t peak_log_memory;
inline uint64_t peak_lnode_memory;
#endif

inline void *pmem_malloc(uint32_t size)
{
	void *ret = malloc(size);
	assert(ret);
	return ret;
}

inline free_log_chunks_t global_log_chunks; // global
inline vlog_group_t *vlog_groups[100];		//每个线程一个
inline log_group_t *log_groups[100];
inline uint32_t log_file_cnt = 0;

inline void log_init()
{
	global_log_chunks.head = (log_chunk_t *)pmem_malloc(sizeof(log_chunk_t *));
	global_log_chunks.head->next = NULL;
	pthread_mutex_init(&(global_log_chunks.lock), NULL);
	for (int i = 0; i <= num_threads; i++)
	{
		vlog_groups[i] = (vlog_group_t *)malloc(sizeof(vlog_group_t));
		vlog_groups[i]->alt = 0;
		log_groups[i] = (log_group_t *)pmem_malloc(sizeof(log_group_t));
		log_groups[i]->alt = 0;
		for (int j = 0; j < 2; j++)
		{
			log_vlog_init(log_groups[i]->log[j], vlog_groups[i]->vlog[j], true);
			vlog_groups[i]->flushed_count[j] = 0;
		}
		clflush(log_groups[i], sizeof(log_group_t));
	}
#ifdef PERF_LOG_MEMORY
	peak_log_memory = 0;
	peak_lnode_memory = 0;
#endif
}

inline uint64_t total_lnode();

inline int get_log_file_cnt(){
	return log_file_cnt;
}

inline bool if_log_recycle()
{
	//统计log总大小。目前用的是log entry为粒度的空间，而不是以chunk为粒度的。
	// uint64_t tot_size = 0;
	// for (int i = 0; i <= num_threads; i++)
	// {
	// 	vlog_t *vlog = &(vlog_groups[i]->vlog[vlog_groups[i]->alt]);
	// 	if (vlog->tot_size > 0)
	// 		tot_size += (vlog->tot_size - LOG_CHUNK_SIZE) + vlog->entry_cnt * LOG_ENTRY_SIZE;
	// }

	uint64_t tot_size = get_log_totsize();

	uint64_t garbage_size = get_flush_totnum() * sizeof(log_group_t);
	// / #ifdef PERF_LOG_MEMORY
	// if (tot_size > total_lnode() * 256 * 0.2)
	// {
	// 	printf("tot_size=%fMB  total_lnode=%fMB", tot_size/1024.0/1024, total_lnode() * 256/1024.0/1024);
	// }
	// #endif

	// return (tot_size > total_lnode() * 256 * 0.2);
	return (tot_size > total_lnode() * 256 * 0.1) && (garbage_size > tot_size * 0.5);
}

#endif

/********************************get pmem space**********************************************/
inline uint64_t freed_nvm_space;
inline uint64_t getNVMusage()
{
	// printf("1111\n");
	return (the_thread_nvmpools.print_usage() - freed_nvm_space);
}

/********************************get dram space*********************************************/
inline uint64_t ini_dram_space;

inline uint64_t getRSS()
{
	FILE *fstats = fopen("/proc/self/statm", "r");
	// the file contains 7 data:
	// vmsize vmrss shared text lib data dt

	size_t buffsz = 0x1000;
	char buff[buffsz];
	buff[buffsz - 1] = 0;
	fread(buff, 1, buffsz - 1, fstats);
	fclose(fstats);
	const char *pos = buff;

	// get "vmrss"
	while (*pos && *pos == ' ')
		++pos;
	while (*pos && *pos != ' ')
		++pos;
	uint64_t rss = atol(pos);

	// get "shared"
	while (*pos && *pos == ' ')
		++pos;
	while (*pos && *pos != ' ')
		++pos;
	uint64_t shared = atol(pos);
	// ull shared = 0;
	//	return rss*4*1024;
	return (rss - shared) * 4 * 1024; // B
}

inline void check_defines()
{
#ifdef GC_NAIVE
	printf("GC_NAIVE\n");
#endif

#ifdef GC_NAIVE_2
	printf("GC_NAIVE_2\n");
#endif

#ifdef PERF_GC
	printf("PERF_GC\n");
#endif

#ifdef INSERT_REPEAT_KEY
	printf("INSERT_REPEAT_KEY\n");
#endif

#ifdef FIXED_BACKGROUND
	printf("FIXED_BACKGROUND\n");
#endif

#ifdef UNIFIED_NODE
	printf("UNIFIED_NODE\n");
#endif

#ifdef PERF_LOG_MEMORY
	printf("PERF_LOG_MEMORY\n");
#endif

#ifdef FLUSH_COUNT
	printf("FLUSH_COUNT\n");
	flush_count_init(num_threads);
#endif

#ifdef eADR_TEST
	printf("eADR_TEST\n");
#endif

#ifdef DRAM_SPACE_TEST
	printf("DRAM_SPACE_TEST\n");
#endif

#ifdef LATENCY_TEST
	printf("LATENCY_TEST\n");
#endif

#ifdef TREE_NO_GC
	printf("TREE_NO_GC\n");
#endif

#ifdef TREE_NO_BUFFER
	printf("TREE_NO_BUFFER\n");
#endif

#ifdef TREE_NO_SELECLOG
	printf("TREE_NO_SELECLOG\n");
#endif

#ifdef TREE_NO_SEARCHCACHE
	printf("TREE_NO_SEARCHCACHE\n");
#endif

#ifdef DO_WARMUP
	printf("DO_WARMUP\n");
#endif

#ifdef DO_INSERT
	printf("DO_INSERT\n");
#endif

#ifdef DO_UPDATE
	printf("DO_UPDATE\n");
#endif

#ifdef DO_SEARCH
	printf("DO_SEARCH\n");
#endif

#ifdef DO_SCAN
	printf("DO_SCAN\n");
#endif

#ifdef DO_DELETE
	printf("DO_DELETE\n");
#endif

#ifdef DO_RECOVERY
	printf("DO_RECOVERY\n");
#endif

#ifdef PERF_GC
	printf("PERF_GC\n");
#endif

#ifdef THRESHOLD_OF_BUFFER_POOL
	printf("THRESHOLD_OF_BUFFER_POOL=%f\n", THRESHOLD_OF_BUFFER_POOL);
#endif
}

/*****************************************************global variable init**********************************/
inline void init_global_variable()
{
	check_defines();

#ifdef MEMPOOL
	worker_id = num_threads; // main thread will share the pmem pool with child thread 0;
#endif
	thread_id = num_threads; // thead id for main thread.

#ifndef FIXED_BACKGROUND
	parallel_merge_worker_num = num_threads;
#endif

	for (int i = 0; i < 100; i++)
	{
#if !defined(GLOBAL_MINILOG) && defined(TREE)
		// minilog_group[i] = (minilog_t *)minilog_create(i);            //v7需要，v8不要
#endif
#if defined(SPLIT_EVALUATION) || defined(SPLIT_BREAKDOWN)
		hist_set_group[i] = new HistogramSet();
#endif
		count_log_group[i] = 0;
		count_lnode_group[i] = 0;
		elapsed_time_group[i] = 0;
		count_search_in_dram[i] = 0;

		count_error_insert[i] = 0;
		count_error_update[i] = 0;
		count_error_search[i] = 0;
		count_error_delete[i] = 0;
		count_error_scan[i] = 0;

		count_conflict_in_bnode[i] = 0;

		create_bnode[i] = 0;
		free_bnode[i] = 0;

		count_access[0][i] = 0;
		count_access[1][i] = 0;
	}

	log_init(); ////////////////

	ini_dram_space = getRSS(); //初始空间
	printf("dram space after init_global_variable: %fMB\n", ini_dram_space / 1024.0 / 1024);

	freed_nvm_space = 0;
	dram_space = 0;

#ifdef LATENCY_TEST
	memset(latency_count, 0, sizeof(latency_count));
	ops_count = 0;
#endif
}

inline uint64_t total_error_insert()
{
	uint64_t sum = 0;
	for (int i = 0; i < 100; i++)
	{
		sum += (count_error_insert[i]);
	}
	return sum;
}

inline uint64_t total_error_update()
{
	uint64_t sum = 0;
	for (int i = 0; i < 100; i++)
	{
		sum += (count_error_update[i]);
	}
	return sum;
}

inline uint64_t total_error_scan()
{
	uint64_t sum = 0;
	for (int i = 0; i < 100; i++)
	{
		sum += (count_error_scan[i]);
	}
	return sum;
}

inline uint64_t total_error_search()
{
	uint64_t sum = 0;
	for (int i = 0; i < 100; i++)
	{
		sum += (count_error_search[i]);
	}
	return sum;
}

inline uint64_t total_error_delete()
{
	uint64_t sum = 0;
	for (int i = 0; i < 100; i++)
	{
		sum += (count_error_delete[i]);
	}
	return sum;
}

inline uint64_t total_error()
{
	uint64_t sum = 0;
	for (int i = 0; i < 100; i++)
	{
		sum += (count_error_insert[i] + count_error_update[i] + count_error_search[i] + count_error_scan[i] + count_error_delete[i]);
	}
	return sum;
}

inline uint64_t total_lnode()
{
	uint64_t sum = 0;
	for (int i = 0; i < 100; i++)
	{
		sum += (count_lnode_group[i]);
	}
	return sum;
}

inline uint64_t total_log()
{
	uint64_t sum = 0;
	for (int i = 0; i < 100; i++)
	{
		sum += (count_log_group[i]);
	}
	return sum;
}

inline uint64_t total_conflict_in_bnode()
{
	uint64_t sum = 0;
	for (int i = 0; i < 100; i++)
	{
		sum += (count_conflict_in_bnode[i]);
	}
	return sum;
}

inline uint64_t total_count_search_in_dram()
{
	uint64_t sum = 0;
	for (int i = 0; i < 100; i++)
	{
		sum += (count_search_in_dram[i]);
	}
	return sum;
}

inline uint64_t total_free_bnode()
{
	uint64_t sum = 0;
	for (int i = 0; i < 100; i++)
	{
		sum += (free_bnode[i]);
	}
	return sum;
}

inline uint64_t total_create_bnode()
{
	uint64_t sum = 0;
	for (int i = 0; i < 100; i++)
	{
		sum += (create_bnode[i]);
	}
	return sum;
}

inline uint64_t total_access(int epoch_num)
{
	uint64_t sum = 0;
	for (int i = 0; i < num_threads; i++)
	{
		sum += (count_access[epoch_num][i]);
	}
	return sum;
}

inline bool if_recycle()
{
	return ((total_log() - pre_total_log) * MINILOG_ITEM_SIZE > total_lnode() * 256 * 0.1);
}

// #if 1
#ifdef LATENCY_TEST
inline void process_latency_data()
{
	uint64_t test_points[110];

	printf("ops_count = %d\n", ops_count);
	int test_points_num = 104;
	for (int i = 1; i <= 99; i++)
	{
		test_points[i] = floor(ops_count / 100.0 * i);
	}
	test_points[100] = floor(ops_count / 100.0 * 99);
	test_points[101] = floor(ops_count / 100.0 * 99.9);
	test_points[102] = floor(ops_count / 100.0 * 99.99);
	test_points[103] = floor(ops_count / 100.0 * 99.999);
	test_points[104] = ops_count;

	uint64_t ops_sum = 0;
	int jnow = 0;
	uint64_t latency_sum = 0;
	for (uint64_t i = 0; i < 10000000; i++)
	{
		for (int j = jnow + 1; j <= test_points_num; j++)
		{
			if (ops_sum < test_points[j])
			{
				if (ops_sum + latency_count[i] >= test_points[j])
				{
					// uint64_t x = test_points[j] - ops_sum;
					printf("latency %d = %llu ns\n", j, i);
					// printf("%llu\n", i);
					jnow = j;
				}
				else
					break;
			}
		}

		ops_sum += latency_count[i];
		latency_sum += i * latency_count[i];
	}

	printf("average latency = %llu ns%\n", latency_sum / ops_count);
	ops_count = 0;
	memset(latency_count, 0, sizeof(latency_count));
}
#endif

#ifdef PERF_GC
inline void perf_gc()
{
	printf("----------perf gc----------\n");
	printf("gc time record:\n");
	for (int i = 0; i < gc_record_idx; i++)
	{
		printf("%llu\n", gc_record[i]);
	}
	printf("\ndo_gc?    ops\n");
	int64_t tmp = 0;
	for (int i = 1; i <= global_time_record; i++)
	{
		int64_t tot = 0;
		for (int j = 0; j < num_threads; j++)
		{
			int64_t addnum = thread_ops_count[j][i] - thread_ops_count[j][i - 1];
			// if (thread_ops_count[j][i] < thread_ops_count[j][i-1])
			// printf("=======thread=%d time=%d %lld  %lld\n", j, i, thread_ops_count[j][i], thread_ops_count[j][i-1]);
			tot += addnum > 0 ? addnum : 0;
		}
		if (is_gc[i])
			printf("true    ");
		else
			printf("false   ");
		printf("%lld\n", tot);
		tmp += tot;
	}

	printf("all ops = %lld\n", tmp);
}
#endif

/***************************************************************************************/

#define likely(x) __builtin_expect(!!(x), 1) // gcc内置函数, 帮助编译器分支优化
#define unlikely(x) __builtin_expect(!!(x), 0)
#define CAS(addr, old_value, new_value) __sync_bool_compare_and_swap(addr, old_value, new_value)

#define setbit(x, y) x |= (1 << y)	//将X的第Y位置1
#define clrbit(x, y) x &= ~(1 << y) //将X的第Y位清0

#ifdef FPTREE
// utils for multi-fptree
#include <stdbool.h>
#include <inttypes.h>
#include <limits.h>
#include <assert.h>

#ifndef __predict_true
#define __predict_true(x) __builtin_expect((x) != 0, 1)
#define __predict_false(x) __builtin_expect((x) != 0, 0)
#endif

#ifndef __constructor
#define __constructor __attribute__((constructor))
#endif

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

#ifndef __aligned
#define __aligned(x) __attribute__((__aligned__(x)))
#endif

#ifndef __unused
#define __unused __attribute__((__unused__))
#endif

#ifndef __arraycount
#define __arraycount(__x) (sizeof(__x) / sizeof(__x[0]))
#endif

#ifndef __UNCONST
#define __UNCONST(a) ((void *)(const void *)a)
#endif

/*
 * Minimum, maximum and rounding macros.
 */

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

/*
 * Byte-order.
 */

#ifndef htobe64
#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#define htobe64(x) OSSwapHostToBigInt64(x)
#endif
#ifdef __linux__
#define _BSD_SOURCE
#include <endian.h>
#endif
#endif

/*
 * A regular assert (debug/diagnostic only).
 */
#if !defined(ASSERT)
#define ASSERT assert
#else
#define ASSERT(x)
#endif
#if defined(NOSMP)
#define NOSMP_ASSERT assert
#else
#define NOSMP_ASSERT(x)
#endif

/*
 * Compile-time assertion: if C11 static_assert() is not available,
 * then emulate it.
 */
#ifndef static_assert
#ifndef CTASSERT
#define CTASSERT(x) __CTASSERT99(x, __INCLUDE_LEVEL__, __LINE__)
#define __CTASSERT99(x, a, b) __CTASSERT0(x, __CONCAT(__ctassert, a), \
										  __CONCAT(_, b))
#define __CTASSERT0(x, y, z) __CTASSERT1(x, y, z)
#define __CTASSERT1(x, y, z) typedef char y##z[(x) ? 1 : -1] __unused
#endif
#define static_assert(exp, msg) CTASSERT(exp)
#endif

/*
 * Atomic operations and memory barriers.  If C11 API is not available,
 * then wrap the GCC builtin routines.
 */

#ifndef atomic_compare_exchange_weak
#define atomic_compare_exchange_weak(ptr, expected, desired) \
	__sync_bool_compare_and_swap(ptr, expected, desired)
#endif
#ifndef atomic_exchange
static inline void *
atomic_exchange(volatile void *ptr, void *nptr)
{
	volatile void *volatile old;

	do
	{
		old = *(volatile void *volatile *)ptr;
	} while (!atomic_compare_exchange_weak(
		(volatile void *volatile *)ptr, old, nptr));

	return (void *)(uintptr_t)old; // workaround for gcc warnings
}
#endif
#ifndef atomic_fetch_add
#define atomic_fetch_add(x, a) __sync_fetch_and_add(x, a)
#endif

#ifndef atomic_thread_fence
/*
 * memory_order_acquire	- membar_consumer/smp_rmb
 * memory_order_release	- membar_producer/smp_wmb
 */
#define memory_order_acquire __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define memory_order_release __atomic_thread_fence(__ATOMIC_RELEASE)
#define atomic_thread_fence(m) m
#endif

/*
 * Exponential back-off for the spinning paths.
 */
#define SPINLOCK_BACKOFF_MIN 4
#define SPINLOCK_BACKOFF_MAX 128
#if defined(__x86_64__) || defined(__i386__)
#define SPINLOCK_BACKOFF_HOOK __asm volatile("pause" :: \
												 : "memory")
#else
#define SPINLOCK_BACKOFF_HOOK
#endif
#define SPINLOCK_BACKOFF(count)              \
	do                                       \
	{                                        \
		int __i;                             \
		for (__i = (count); __i != 0; __i--) \
		{                                    \
			SPINLOCK_BACKOFF_HOOK;           \
		}                                    \
		if ((count) < SPINLOCK_BACKOFF_MAX)  \
			(count) += (count);              \
	} while (/* CONSTCOND */ 0);

/*
 * Cache line size - a reasonable upper bound.
 */
#define CACHE_LINE_SIZE 64
#endif