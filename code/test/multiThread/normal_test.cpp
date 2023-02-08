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
    if (argc != 4)
    {
        fprintf(stderr, "The parameters (num_keys and num_threads and scan_size) are required\n");
        return 0;
    }

    num_keys = atoi(argv[1]);
    printf("n=%lu\n", num_keys);
    num_threads = atoi(argv[2]);
    mscan_size = atoi(argv[3]);
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

    printf("after key init: dram space (RSS) = %fMB\n", (getRSS() - ini_dram_space) / 1024.0 / 1024);

    // std::shuffle(keys, keys + num_keys, eng);
    uint64_t time_start;

/************************************ global variable*************************************/
#ifndef NUMA_TEST
    openPmemobjPool("/mnt/pmem/sobtree/leafdata", 40ULL * 1024ULL * 1024ULL * 1024ULL, num_threads); // 单节点
#else
    // numa 多节点
    openPmemobjPool("/mnt/pmem/sobtree/leafdata", "/pmem/sobtree/leafdata", 40ULL * 1024ULL * 1024ULL * 1024ULL);
#endif
    tree_init();

    printf("after tree_init() : dram space (RSS) = %fMB\n", (getRSS() - ini_dram_space) / 1024.0 / 1024);

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

#if (defined(TREE) || defined(TREEFF)) // 都做一次垃圾回收
#ifndef TREE_NO_GC
    while (!CAS(&signal_do_recycle, false, true))
    {
    };
    while (signal_do_recycle == true)
    {
    };
    printf("log recycle for warmming up! begin recycle\n");
#endif

    printf("conflict_in_bnode = %lld\n", total_conflict_in_bnode());
#endif

#ifdef DPTREE
    printf("wait for background..\n");
    while (bt->is_merging())
        ;
#endif

    printf("dram space after warmup: %fMB\n", getRSS() / 1024.0 / 1024);

    printf("create bnode = %llu, free bnode = %llu.\n", total_create_bnode(), total_free_bnode());

#endif // DO_WARMUP

#ifdef FLUSH_COUNT
    not_warmup = true;
#endif

    //***************************insert op*******************************//
#ifdef DO_INSERT

#ifdef PERF_GC
    while (signal_do_recycle == true) // 等待可能的GC结束。
    {
    };
    sfence();
    printf("clean bn! %d\n", signal_do_recycle);
    tree_recycle(); // 主线程清空bnode
    printf("clean end!\n");
    // 初始化
    global_time_record = 0;
    for (int i = 0; i < 50; i++)
        for (int j = 0; j < GC_RECORD_CNT + 1; j++)
            thread_ops_count[i][j] = 0;
    // memset(thread_ops_count, 0, sizeof(thread_ops_count));
    gc_record_idx = 0;
    printf("begin insert!\n");
#endif
#ifndef MIXED_WORKLOAD
    clear_cache();
    futures.clear();

#ifdef FLUSH_COUNT_USE_IPMCTL
    // _mm_mfence();
    // // system("sudo ipmctl show -dimm 0x0010,0x0110,0x0210,0x0310 -performance");
    // my_system("sudo ipmctl show -dimm 0x0010,0x0110,0x0210,0x0310 -performance");
    _mm_mfence();
    // printf("在另一个命令行窗口第一次运行：sudo ipmctl show -dimm 0x0010,0x0110,0x0210,0x0310 -performance\n"); //numa 0
    printf("在另一个命令行窗口第一次运行：sudo ipmctl show -dimm 0x1010,0x1110,0x1210,0x1310 -performance\n"); // numa 1
    char tep_char = getchar();
#endif

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

#ifdef PACTREE
    tree_get_memory_footprint();
#endif

#ifdef PERF_GC
    perf_gc();
#endif

#if (defined(TREE) && !defined(GC_NAIVE) && !defined(GC_NAIVE_2))
    printf("conflict_in_bnode = %lld\n", total_conflict_in_bnode());
#ifndef TREE_NO_GC
    // while (!CAS(&signal_do_recycle, false, true))
    // {
    // };
    while (signal_do_recycle == true)
    {
    };
    printf("log recycle for insert! begin recycle\n");
#endif
#endif

#ifdef DPTREE
    printf("wait for background..\n");
    while (bt->is_merging())
        ;
#endif

    printf("dram space after insert: %fMB\n", getRSS() / 1024.0 / 1024);

    printf("create bnode = %llu, free bnode = %llu.\n", total_create_bnode(), total_free_bnode());

#ifdef LATENCY_TEST
    process_latency_data();
#endif

#endif // DO_INSERT
    // tree->printAll();
    // printf("\n\n\n*************\n\n\n");
    // tree->printinfo_leaf();
    // for(int i=0;i<5;i++)
    // {
    //     while (!CAS(&signal_do_recycle, false, true))
    //     {
    //     };
    //     while (signal_do_recycle == true)
    //     {
    //     };
    //     // printf("log recycle for warmming up! begin recycle\n");
    //     printf("create bnode = %llu, free bnode = %llu.\n", total_create_bnode(), total_free_bnode());
    // }

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
#ifdef TREE
#ifndef TREE_NO_GC
    // while (!CAS(&signal_do_recycle, false, true))
    // {
    // };
    while (signal_do_recycle == true)
    {
    };
    printf("log recycle for update! begin recycle\n");
#endif
#endif

#endif // end update

    // #ifdef TREE
    //     //关闭背景线程
    //     signal_run_bgthread = false;
    //     bg_thread.get();
    // #endif

#ifdef DPTREE
    printf("wait for background..\n");
    while (bt->is_merging())
        ;
#endif
#endif // DO_UPDATE

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

#ifdef DPTREE
    printf("wait for background..\n");
    while (bt->is_merging())
        ;
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

#ifdef DPTREE
    printf("wait for background..\n");
    while (bt->is_merging())
        ;
#endif

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

#ifdef DPTREE
    printf("wait for background..\n");
    while (bt->is_merging())
        ;
#endif

#endif // DO_DELETE

        //***************************recovery op*******************************//
#ifdef DO_RECOVERY
    time_start = NowNanos();
#ifdef LBTREE
    lbtree *lb_recovery = tree_recovery(num_threads);
#endif
#ifdef TREE
    signal_run_bgthread = false;
    bg_thread.get();

    tree *tr_recovery = tree_recovery(num_threads + 1, num_threads); // num_threadss
#endif
    printf("%d threads recovery time cost is %llu ns. \n", num_threads, ElapsedNanos(time_start));

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
                    // tree_search(keys[i]);
                    key_type_sob key = keys[i];

#ifdef LBTREE
                    uint64_t index = 0;
                    bleaf *lp = (bleaf *)lb_recovery->lookup(key, &index);
                    value_type_sob res = lp->ch(index).value;
#endif // lbtree
#ifdef TREE
                    uint64_t res = tr_recovery->search_lnode(key);
#endif // tree
                    if (unlikely(res != key))
                    {
                        printf("error after recovery:  key = %llu res = %llu\n", key, res);
                        // count_error_search[thread_id]++;
                    }
                }
            },
            from, to, tid);
        futures.push_back(move(f));
    }
    for (auto &&f : futures)
        if (f.valid())
            f.get();
    printf("%d threads search time cost is %llu ns. error_count = %lld\n", num_threads, ElapsedNanos(time_start), total_error_search());

#endif // ifdef recovery

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

#if defined(BACKGROUND) && defined(SOBTREE)
    // printf("\n******* total = %d, actual = %d, inode = %d *******\n", bt->num_total_keys, bt->num_actual_keys, bt->num_total_inodes);
    bt->printinfo_bottom();
    // printf("count flush :%d\n", count_flush);
    // printf("to_dram = %d,out_dram = %d", to_dram_group[0], out_dram_group[0]);
#endif
#ifdef TREE
    // printf("total_log = %llu, total_lnode = %llu\n", total_log(), total_lnode());
#endif

    /*****************printf some information***********************/

#ifdef FLUSH_COUNT
    // fclose(file_fc);
    flush_count_process();
    printf("count_flush = %llu   count_flush_256 = %llu\n", count_flush, count_flush_256);
    // printf("count_flush_log = %llu   count_flush_tree = %llu\n", count_flush_log, count_flush_tree);
#endif

#ifdef FLUSH_COUNT_USE_IPMCTL
    _mm_mfence();
    // system("sudo ipmctl show -dimm 0x0010,0x0110,0x0210,0x0310 -performance"); // numa 0
    system("sudo ipmctl show -dimm 0x1010,0x1110,0x1210,0x1310 -performance"); // numa 1
    _mm_mfence();
#endif

#ifdef PERF_LOG_MEMORY
    printf("peak log memory = %f MB\n", peak_log_memory / 1024.0 / 1024);
    printf("peak node memory = %f MB\n", peak_lnode_memory / 1024.0 / 1024);
    printf("end node memory = %f MB\n", total_lnode() * 256 / 1024.0 / 1024);
    printf("nvm log file need = %d\n", get_log_file_cnt());
#endif

    // 空间开销统计
    printf("nvm space = %fMB\n", getNVMusage() / 1024.0 / 1024);
    // printf("nowRSS=%fMB  iniRSS=%fMB\n", getRSS()/1024.0/1024, ini_dram_space/1024.0/1024);
    printf("dram space (RSS) = %fMB\n", (getRSS() - ini_dram_space) / 1024.0 / 1024);

#ifdef DRAM_SPACE_TEST
    printf("dram space (non_lnode_space) = %fMB\n", dram_space / 1024.0 / 1024);
#endif

    printf("create bnode = %llu, free bnode = %llu.\n", total_create_bnode(), total_free_bnode());

    // #if defined(FASTFAIR) || defined(DPTREE)
    //     tree_end();
    //     printf("after ~tree(): dram space (RSS) = %fMB\n", (getRSS()-ini_dram_space)/1024.0/1024 );
    // #endif

#if defined(FLATSTORE) || defined(TREE)
    printf("log_totsize = %fMB\n", get_log_totsize() / 1024.0 / 1024);
#endif

    // printf("count in dram = %llu.\n", count_search_in_dram[0]);
    // bt->printinfo_bnode();
    printf("\n");
    // bt->printinfo_leaf();
    // printf("\n");
    // time_start = NowNanos();
    // bt->recycle_bottom();
    // uint64_t ll = ElapsedNanos(time_start);
    // bt->printinfo_leaf();
    // bt->printinfo_bnode();
    printf("\n");
    // bt->printinfo_leaf();
    // printf("\n");
    fence();
    // fprintf(stderr, "%d threads mixed time cost is %llu\n", num_threads, ll);
    // printf("\n************\n");
    fence();
    // bt->printinfo();

#ifdef TREE
#ifndef TREE_NO_GC
    signal_run_bgthread = false;
    bg_thread.get();
#endif
#endif
    // _mm_mfence();
    // while (true)
    // {
    //     printf("signal_do_recycle = %d signal_run_bgthread=%d \n", signal_do_recycle, signal_run_bgthread);
    //     ;
    // }

#ifdef DPTREE
    tree_end();
#endif

#ifdef TREEFF
#ifndef TREE_NO_GC
    signal_run_bgthread = false;
    bg_thread.get();
#endif
#endif

    return 0;
}
