#include "interface.h"
#include "util.h"

using namespace std;

void clear_cache()
{
    // Remove cache
    uint64_t size = 256 * 1024 * 1024;
    char *garbage = new char[size];
    for (uint64_t i = 0; i < size; ++i)
        garbage[i] = i;
    for (uint64_t i = 100; i < size; ++i)
        garbage[i] += garbage[i - 100];
    delete[] garbage;
}

int main(int argc, char **argv)
{
    //***************************init op*******************************//
    // generate keys
    if (argc != 3)
    {
        fprintf(stderr, "The parameters (num_keys and num_threads and scan_size) are required\n");
        return 0;
    }

    num_keys = atoi(argv[1]);
    printf("n=%lu\n", num_keys);
    num_threads = atoi(argv[2]);
    mscan_size = 100;
    mmax_length_for_scan = mscan_size + 64;
    printf("mscan_size=%d  mmax_length_for_scan=%d\n", mscan_size, mmax_length_for_scan);

    init_global_variable(); /////////////init global移到这里了/////////////////

    key_type_sob *keys = (key_type_sob *)malloc(num_keys * sizeof(key_type_sob));
    assert(keys);
    std::random_device rd;
    std::mt19937_64 eng(rd());

    // std::uniform_int_distribution<key_type_sob> uniform_dist(1, num_keys);
    std::uniform_int_distribution<key_type_sob> uniform_dist;
#ifndef ZIPFIAN
    for (uint64_t i = 0; i < num_keys;)
    {
        key_type_sob x = uniform_dist(eng);
        if (x > 0 && x < INT64_MAX - 1)
        {
            keys[i++] = x;
            // keys[i++] = i + 1;
        }
        else
        {
            continue;
        }
    }
#else
    ZipfianGenerator zf(num_keys);
    printf("zipfian distribution\n");
    for (uint64_t i = 0; i < num_keys; i++)
    {
        keys[i] = zf.Next() + 1;
    }
#endif

    // std::shuffle(keys, keys + num_keys, eng);
    uint64_t time_start;

    /************************************ global variable*************************************/
    openPmemobjPool("/mnt/pmem/sobtree/leafdata", 40ULL * 1024ULL * 1024ULL * 1024ULL, num_threads); // 单节点

    tree_init();

    //***************************multi thread init**********************//
    std::vector<std::future<void>> futures(num_threads);
    uint64_t data_per_thread = (num_keys / 2) / num_threads;

    //***************************warm up**********************//
#ifdef DO_WARMUP
    time_start = NowNanos();
    for (uint64_t tid = 0; tid < num_threads; tid++)
    {

        uint64_t from = data_per_thread * tid;
        uint64_t to = (tid == num_threads - 1) ? num_keys / 2 : from + data_per_thread;
        auto f = async(
            launch::async,
            [&](uint64_t from, uint64_t to, uint64_t tid)
            {
#ifdef PIN_CPU
                pin_cpu_core(tid);
#endif

#ifdef MEMPOOL
                worker_id = tid;
                thread_id = tid;
#endif

                for (uint64_t i = from; i < to; ++i)
                {
                    tree_insert(keys[i]);
                    // printf("%llu\n",keys[i]);
                    // tree->printAll();
                }
            },
            from, to, tid);
        futures.push_back(move(f));
    }
    for (auto &&f : futures)
        if (f.valid())
        {
            f.get();
            // printf("111\n");
        }
    printf("%d threads warm up time cost is %llu ns. error_count = %lld\n", num_threads, ElapsedNanos(time_start), total_error_insert());

#endif // DO_WARMUP

#ifdef FLUSH_COUNT
    not_warmup = true;
#endif

    //***************************insert op*******************************//
#ifdef DO_INSERT

    time_start = NowNanos();

    for (uint64_t tid = 0; tid < num_threads; tid++)
    {
        uint64_t from = data_per_thread * tid + num_keys / 2;
        uint64_t to = (tid == num_threads - 1) ? num_keys : from + data_per_thread;

#ifdef PERF_GC
        // 初始化
        thread_time_record = 0;
        thread_ops_count[tid][0] = from;
#endif

        auto f = async(
            launch::async,
            [&](uint64_t from, uint64_t to, uint64_t tid)
            {
#ifdef PIN_CPU
                pin_cpu_core(tid);
#endif

#ifdef MEMPOOL
                worker_id = tid;
                thread_id = tid;

#endif

                for (uint64_t i = from; i < to; ++i)
                {
#ifdef PERF_GC
                    if (i % 100 == 0)
                    {
                        if (thread_time_record != global_time_record)
                        {
                            // printf("%llu %llu\n", thread_time_record, global_time_record);
                            thread_ops_count[tid][global_time_record] = i;
                            thread_time_record = global_time_record;
                        }
                    }

#endif

#ifdef LATENCY_TEST
                    if (tid == 0) // && (i % 10 == 0))
                    {
                        uint64_t time_start_insert = NowNanos();
                        tree_insert(keys[i]);
                        uint64_t time_pass = ElapsedNanos(time_start_insert);
                        if (time_pass < 10000000) // 10ms
                        {
                            latency_count[time_pass]++;
                            ops_count++;
                            // printf("%llu\n", time_pass);
                        }
                        else
                            printf("!! %llu\n", time_pass);
                    }
                    else
                    {
                        tree_insert(keys[i]);
                    }
#else
                    tree_insert(keys[i]);
                // bt->printinfo_bnode();
#endif
                }
            },
            from, to, tid);
        futures.push_back(move(f));
    }

#ifdef PERF_GC
    while (true)
    {
        usleep(GC_RECORD_INTERVAL); // us
        // usleep(GC_RECORD_INTERVAL); //us
        if (global_time_record + 1 == GC_RECORD_CNT)
            break;
        is_gc[++global_time_record] = signal_do_recycle;
    }
#endif

    for (auto &&f : futures)
        if (f.valid())
            f.get();
    printf("%d threads insert time cost is %llu ns. error_count = %lld\n", num_threads, ElapsedNanos(time_start), total_error_insert());

#ifdef LATENCY_TEST
    process_latency_data();
#endif

#endif // DO_INSERT

    //***************************update op*******************************//
#ifdef DO_UPDATE

#ifdef SHUFFLE_KEYS
    std::shuffle(keys, keys + num_keys, eng);
#endif

    clear_cache();
    futures.clear();
    time_start = NowNanos();

    for (uint64_t tid = 0; tid < num_threads; tid++)
    {
        uint64_t from = data_per_thread * tid + num_keys / 2;
        uint64_t to = (tid == num_threads - 1) ? num_keys : from + data_per_thread;

        auto f = async(
            launch::async,
            [&](uint64_t from, uint64_t to, uint64_t tid)
            {
#ifdef PIN_CPU
                pin_cpu_core(tid);
#endif

#ifdef MEMPOOL
                worker_id = tid;
                thread_id = tid;

#endif

                for (uint64_t i = from; i < to; ++i)
                {
                    tree_update(keys[i]);
                }
            },
            from, to, tid);
        futures.push_back(move(f));
    }
    for (auto &&f : futures)
        if (f.valid())
            f.get();
    printf("%d threads update time cost is %llu ns. error_count = %lld\n", num_threads, ElapsedNanos(time_start), total_error_update());
    // printf("count flush :%d\n", count_flush);

#endif // DO UPDATE

    //***************************search op*******************************//
#ifdef DO_SEARCH

#ifdef SHUFFLE_KEYS
    std::shuffle(keys, keys + num_keys, eng);
#endif

#ifdef SEARCH_PERF
    printf("SEARCH_PERF\n");
    extern uint64_t time_search_dram;
    extern uint64_t time_search_pmem;
    time_search_dram = 0;
    time_search_pmem = 0;
#endif

    clear_cache();
    futures.clear();
    time_start = NowNanos();

    for (uint64_t tid = 0; tid < num_threads; tid++)
    {
        uint64_t from = data_per_thread * tid + num_keys / 2;
        uint64_t to = (tid == num_threads - 1) ? num_keys : from + data_per_thread;

        auto f = std::async(
            std::launch::async,
            [&](uint64_t from, uint64_t to, uint64_t tid)
            {
#ifdef PIN_CPU
                pin_cpu_core(tid);
#endif
                thread_id = tid;
                for (uint64_t i = from; i < to; ++i)
                {
#ifdef LATENCY_TEST
                    uint64_t time_start_insert = NowNanos();
                    tree_search(keys[i]);
                    if (tid == 0) // && i % 10 == 0)
                    {
                        uint64_t time_pass = ElapsedNanos(time_start_insert);
                        if (time_pass < 10000000) // 10ms
                        {
                            latency_count[time_pass]++;
                            ops_count++;
                            // printf("%llu\n", time_pass);
                        }
                        else
                            printf("!! %llu\n", time_pass);
                    }
#else
                    tree_search(keys[i]);
#endif
                }
            },
            from, to, tid);
        futures.push_back(move(f));
    }
    for (auto &&f : futures)
        if (f.valid())
            f.get();
    printf("%d threads search time cost is %llu ns. error_count = %lld\n", num_threads, ElapsedNanos(time_start), total_error_search());
    printf("search hit in dram = %d \n", total_count_search_in_dram());

#ifdef SEARCH_PERF
    printf("time in dram: %llu ns.\ntime in pmem: %llu ns.\n", time_search_dram, time_search_pmem);
#endif

#ifdef LATENCY_TEST
    process_latency_data();
#endif

#endif // DO_SEARCH

    //***************************scan op*******************************//
#ifdef DO_SCAN

#ifdef SHUFFLE_KEYS
    std::shuffle(keys, keys + num_keys, eng);
#endif

    clear_cache();
    futures.clear();
    time_start = NowNanos();

    for (uint64_t tid = 0; tid < num_threads; tid++)
    {
        uint64_t from = data_per_thread * tid + num_keys / 2;
        uint64_t to = (tid == num_threads - 1) ? num_keys : from + data_per_thread;

        auto f = std::async(
            std::launch::async,
            [&](uint64_t from, uint64_t to, uint64_t tid)
            {
#ifdef PIN_CPU
                pin_cpu_core(tid);
#endif
                thread_id = tid;

                std::vector<value_type_sob> buf;
                buf.reserve(mmax_length_for_scan);
                for (uint64_t i = from; i < to; ++i)
                {

                    tree_scan(keys[i], mscan_size, buf);
                    buf.clear();
                }
            },
            from, to, tid);
        futures.push_back(move(f));
    }
    for (auto &&f : futures)
        if (f.valid())
            f.get();
    printf("%d threads scan time cost is %llu ns. error_count = %lld\n", num_threads, ElapsedNanos(time_start), total_error_scan());

#endif // DO_SCAN

    //***************************delete op*******************************//
#ifdef DO_DELETE

#ifdef SHUFFLE_KEYS
    std::shuffle(keys, keys + num_keys, eng);
#endif

    clear_cache();
    futures.clear();
    time_start = NowNanos();

    for (uint64_t tid = 0; tid < num_threads; tid++)
    {
        uint64_t from = data_per_thread * tid + num_keys / 2;
        uint64_t to = (tid == num_threads - 1) ? num_keys : from + data_per_thread;
        // printf("thread %d , from = %d ,to = %d\n", tid, from, to);
        auto f = async(
            launch::async,
            [&](uint64_t from, uint64_t to, uint64_t tid)
            {
#ifdef PIN_CPU
                pin_cpu_core(tid);
#endif

#ifdef MEMPOOL
                worker_id = tid;
                thread_id = tid;

#endif
                for (uint64_t i = from; i < to; ++i)
                {
                    tree_delete(keys[i]);
                }
            },
            from, to, tid);
        futures.push_back(move(f));
    }
    for (auto &&f : futures)
        if (f.valid())
            f.get();
    printf("%d threads delete time cost is %llu ns. error_count = %lld\n", num_threads, ElapsedNanos(time_start), total_error_delete());

#endif // DO_DELETE

    free(keys);
#ifdef SPLIT_EVALUATION
    for (uint64_t tid = 0; tid < 1; tid++)
    {
        hist_set_group[tid]->PrintResult();
        // printf("thread %d : insert elapsed time = %llu\n", tid, elapsed_time_group[tid]);
    }
#endif

#ifdef SPLIT_BREAKDOWN
    hist_set_group[0]->PrintResult();
#endif

    return 0;
}
