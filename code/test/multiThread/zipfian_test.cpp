#include "interface.h"
#include "util.h"

using namespace std;

void clear_cache()
{
    // Remove cache
    int size = 256 * 1024 * 1024;
    char *garbage = new char[size];
    for (int i = 0; i < size; ++i)
        garbage[i] = i;
    for (int i = 100; i < size; ++i)
        garbage[i] += garbage[i - 100];
    delete[] garbage;
}

int main(int argc, char **argv)
{
    //***************************init op*******************************//

    // generate keys
    if (argc != 4)
    {
        fprintf(stderr, "The parameters (num_keys and num_threads and zipfian_attri) are required\n");
        return 0;
    }
    num_keys = atoi(argv[1]);
    printf("n=%lu\n", num_keys);
    num_threads = atoi(argv[2]);
    key_type_sob *keys_insert = (key_type_sob *)malloc(num_keys * sizeof(key_type_sob));
    key_type_sob *keys_search = (key_type_sob *)malloc(num_keys * sizeof(key_type_sob));
    std::random_device rd;
    std::mt19937_64 eng(rd());

    double zipfian_attri = stod(argv[3]);
    ycsbc::ScrambledZipfianGenerator zf(0, num_keys - 1, zipfian_attri);
    printf("zipfian distribution\n");
    for (uint64_t i = 0; i < num_keys; i++)
    {
        keys_insert[i] = zf.Next() + 1;
        // keys_search[i] = keys_insert[i];
    }

    std::shuffle(keys_insert, keys_insert + num_keys, eng);

    std::shuffle(keys_search, keys_search + num_keys, eng);

    // for (uint64_t i = 0; i < num_keys; i++)
    // {
    //     printf(" %llu ,%llu\n", keys_insert[i], keys_search[i]);
    // }



    uint64_t time_start;
    
    init_global_variable();


    /************************************ global variable*************************************/
#ifndef NUMA_TEST
    openPmemobjPool("/mnt/pmem/sobtree/leafdata", 40ULL * 1024ULL * 1024ULL * 1024ULL, num_threads); //单节点
#else
    // numa 多节点
    openPmemobjPool("/mnt/pmem/sobtree/leafdata", "/pmem/sobtree/leafdata", 40ULL * 1024ULL * 1024ULL * 1024ULL);
#endif

    tree_init();

    //***************************multi thread init**********************//
    std::vector<std::future<void>> futures(num_threads);
    uint64_t data_per_thread = (num_keys / 2) / num_threads; // 50/2=25

    //***************************warm up**********************//
    time_start = NowNanos();
    for (int tid = 0; tid < num_threads; tid++)
    {
        int from = data_per_thread * tid;
        int to = (tid == num_threads - 1) ? num_keys / 2 : from + data_per_thread;
        // printf("thread %d , from = %d ,to = %d\n", tid, from, to);
        auto f = async(
            launch::async,
            [&](int from, int to, int tid)
            {
#ifdef PIN_CPU
                pin_cpu_core(tid);
#endif

#ifdef MEMPOOL
                worker_id = tid;
                thread_id = tid;
#endif

                for (int i = from; i < to; ++i)
                {

                    tree_insert(keys_insert[i]);
                    // tree_search(keys_search[i]);
                    // printf("**%llu\t", keys[i]);
                    // bt->printinfo_bnode();
                    // printf("\t");
                    // bt->printinfo_leaf();
                    // printf("\n");
                }
            },
            from, to, tid);
        futures.push_back(move(f));
    }
    for (auto &&f : futures)
        if (f.valid())
            f.get();
    printf("%d threads warm up time cost is %llu ns. error_count = %lld\n", num_threads, ElapsedNanos(time_start), total_error_insert());

#if (defined(TREE) || defined(TREEFF)) //都做一次垃圾回收
    while (!CAS(&signal_do_recycle, false, true))
    {
    };
    while (signal_do_recycle == true)
    {
    };
    printf("log recycle for warmming up! begin recycle\n");

    printf("conflict_in_bnode = %lld\n", total_conflict_in_bnode());

#endif

#ifdef DPTREE
    printf("wait for background..\n");
    while (bt->is_merging())
        ;
#endif

#ifdef FLUSH_COUNT
    not_warmup = true;
#endif

    //***************************insert op*******************************//

    clear_cache();
    futures.clear();

#ifdef FLUSH_COUNT_USE_IPMCTL
    // _mm_mfence();
    // system("sudo ipmctl show -dimm 0x0010,0x0110,0x0210,0x0310 -performance");
    // _mm_mfence();
    // printf("在另一个命令行窗口第一次运行：sudo ipmctl show -dimm 0x0010,0x0110,0x0210,0x0310 -performance\n"); //numa 0
    printf("在另一个命令行窗口第一次运行：sudo ipmctl show -dimm 0x1010,0x1110,0x1210,0x1310 -performance\n"); // numa 1
    char tep_char = getchar();
#endif

    time_start = NowNanos();

#ifndef FLUSH_COUNT
    data_per_thread /= 2; // 50+25+25
#endif

    for (int tid = 0; tid < num_threads; tid++)
    {
        int from = data_per_thread * tid + num_keys / 2;
#ifdef FLUSH_COUNT
        int to = (tid == num_threads - 1) ? num_keys : from + data_per_thread;
#else
        int to = std::min(num_keys, from + data_per_thread); // 50+25+25
#endif
        // printf("to-from = %d\n", to-from);

        // printf("thread %d , from = %d ,to = %d\n", tid, from, to);
        auto f = async(
            launch::async,
            [&](int from, int to, int tid)
            {
#ifdef PIN_CPU
                pin_cpu_core(tid);
#endif

#ifdef MEMPOOL
                worker_id = tid;
                thread_id = tid;

#endif

                for (int i = from; i < to; ++i)
                {
                    tree_insert(keys_insert[i]);
#ifndef FLUSH_COUNT
                    tree_search(keys_insert[i - num_keys / 2]); // 50+25+25
#endif
                    // printf("**%llu\t", keys[i]);
                    // bt->printinfo_bnode();
                    // printf("\t");
                    // bt->printinfo_leaf();
                    // printf("\n");
                }
            },
            from, to, tid);
        futures.push_back(move(f));
    }
    for (auto &&f : futures)
        if (f.valid())
            f.get();
    printf("%d threads zipfian time cost is %llu ns. error_count = %lld\n", num_threads, ElapsedNanos(time_start), total_error_insert() + total_error_search());
    // printf("count flush :%d\n", count_flush);

    printf("conflict_in_bnode = %lld\n", total_conflict_in_bnode());
#ifdef TREE
    while (!CAS(&signal_do_recycle, false, true))
    {
    };
    while (signal_do_recycle == true)
    {
    };
    printf("log recycle for insert! begin recycle\n");
#endif

#ifdef DPTREE
    printf("wait for background..\n");
    while (bt->is_merging())
        ;
#endif

    free(keys_insert);
    free(keys_search);
    /*****************printf some information***********************/

#ifdef FLUSH_COUNT
    // fclose(file_fc);
    flush_count_process();
    printf("count_flush = %llu   count_flush_256 = %llu\n", count_flush, count_flush_256);
    // printf("count_flush_log = %llu   count_flush_tree = %llu\n", count_flush_log, count_flush_tree);
#endif

#ifdef FLUSH_COUNT_USE_IPMCTL
    _mm_mfence();
    // system("sudo ipmctl show -dimm 0x0010,0x0110,0x0210,0x0310 -performance"); //numa 0
    system("sudo ipmctl show -dimm 0x1010,0x1110,0x1210,0x1310 -performance");
    _mm_mfence();
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
    signal_run_bgthread = false;
    bg_thread.get();
#endif

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
