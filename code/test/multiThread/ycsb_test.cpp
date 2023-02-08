#include "interface.h"

// addr is the memory addr that want to prefetch.
// rw: 0 means prefetch for read, 1 means prefetch for write.
// pri: 0 means this addr need not be left in the cache after the access.
//      1 means low priority after the access.
//      2 means moderate priority after the access.
//      3 means high priority after the access.
#define PREFETCH(addr, rw, pri) __builtin_prefetch((addr), (rw), (pri))

#define LIKELY(x) __builtin_expect((x), 1)
#define UNLIKELY(x) __builtin_expect((x), 0)

#define OP_UNKNOWN 0x0
#define OP_INSERT 0x1
#define OP_READ 0x2
#define OP_UPDATE 0x3
#define OP_SCAN 0x4

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

inline uint8_t parse_op_type(std::string raw)
{
    if (raw == "INSERT")
    {
        return OP_INSERT;
    }
    else if (raw == "READ")
    {
        return OP_READ;
    }
    else if (raw == "UPDATE")
    {
        return OP_UPDATE;
    }
    else if (raw == "SCAN")
    {
        return OP_SCAN;
    }
    else
    {
        return OP_UNKNOWN;
    }
}

struct ycsb_Operation
{
    uint64_t key_ = 0;
    uint8_t type_ = OP_UNKNOWN;
    uint8_t next_ = 0; // this is only used for scan op.

    ycsb_Operation(std::string raw_line)
        : key_(0),
          type_(OP_UNKNOWN),
          next_(0)
    {
        std::istringstream iss(raw_line);
        std::string token;
        int flag = 0;
        while (std::getline(iss, token, ';'))
        {
            if (flag == 0 && token.size() > 0)
            {
                type_ = parse_op_type(token);
            }
            else if (flag == 1 && token.size() > 0)
            {
                key_ = std::stoull(token);
                // printf("%llu\n", key_);
            }
            else if (type_ == OP_SCAN &&
                     flag == 2 &&
                     token[0] != '\r' &&
                     token[0] != '\n')
            {
                next_ = std::stoi(token);
            }
            else
            {
                break;
            }
            ++flag;
        }
    }

#ifndef NDEBUG
    void Show()
    {
        std::string str = "";
        switch (type_)
        {
        case OP_INSERT:
            str += "INSERT;";
            break;
        case OP_READ:
            str += "READ;";
            break;
        case OP_UPDATE:
            str += "UPDATE;";
            break;
        case OP_SCAN:
            str += "SCAN;";
            break;
        default:
            break;
        }
        str += std::to_string(key_) + ";";
        if (type_ == OP_SCAN)
        {
            str += std::to_string(next_) + ";";
        }
    }
#endif
};

class DataSet
{
public:
    explicit DataSet(std::string fname)
    {
        printf("%s\n", fname.c_str());
        std::ifstream infile(fname);
        std::string line;
        ops_.reserve(100000000);
        while (std::getline(infile, line))
        {
            // printf("%s\n", line.c_str());
            if (line[0] == '#')
            {
                break;
            }
            ops_.emplace_back(ycsb_Operation(line));
        }
        infile.close();
    }

    DataSet() {}

    DataSet(const DataSet &) = delete;
    DataSet(const DataSet &&) = delete;
    ~DataSet(){};

    DataSet(const DataSet *other, uint64_t from, uint64_t to)
    {
        from = (from >= other->ops_.size()) ? other->ops_.size() : from;
        to = (to >= other->ops_.size()) ? other->ops_.size() : to;
        ops_.reserve(to - from + 10);
        for (uint64_t i = from; i < to; ++i)
        {
            ops_.push_back(other->ops_[i]);
        }
    }

    void Perpare()
    {
        cursor_ = ops_.begin();
        PREFETCH(&(ops_[0]), 0, 1);
    }

    inline bool Valid()
    {
        return (cursor_ != ops_.end());
    }

    inline int OpType()
    {
        return cursor_->type_;
    }

    inline uint64_t Key()
    {
        return cursor_->key_;
    }

    inline int ScanCount()
    {
        return cursor_->next_;
    }

    inline void Next()
    {
        ++cursor_;
    }

    size_t Size()
    {
        return ops_.size();
    }

#ifndef NDEBUG
    void Show()
    {
        std::vector<ycsb_Operation>::iterator it = ops_.begin();
        while (it != ops_.end())
        {
            it->Show();
            ++it;
        }
    }
#endif

private:
    std::vector<ycsb_Operation>::iterator cursor_;
    std::vector<ycsb_Operation> ops_;
};

void TestFunc(DataSet *set, int tid)
{
#ifdef PIN_CPU
    pin_cpu_core(tid);
#endif
#ifdef MEMPOOL
    worker_id = tid;
    thread_id = tid;
#endif
    std::vector<key_type_sob> buf;
    buf.resize(200); // MAX_LENGTH_FOR_SCAN
    set->Perpare();
    while (set->Valid())
    {
        switch (set->OpType())
        {
        case OP_INSERT:
            tree_insert(set->Key());
            break;
        case OP_READ:
            tree_search(set->Key());
            break;
        case OP_UPDATE:
            tree_update(set->Key());
            break;
        case OP_SCAN:
            tree_scan(set->Key(), set->ScanCount(), buf);
            buf.clear();
            break;
        default:;
        }
        set->Next();
    }
}

void RunYCSBBench(DataSet *load_data, DataSet *run_data, int load_thread_num, int run_thread_num)
{

    std::vector<std::thread> thread_vec;
    uint64_t time_start;
    ///////////////////////////////////////////////////
    // generate load data
    // only insert key in load phase
    ///////////////////////////////////////////////////
    uint64_t load_pre_thead = load_data->Size() / load_thread_num;
    // printf("load_pre_thead: %lu * %d -> %zu\n", load_pre_thead, load_thread_num, load_data->Size());
    DataSet *load_set[load_thread_num];
    for (int tid = 0; tid < load_thread_num; tid++)
    {
        uint64_t from = tid * load_pre_thead;
        uint64_t to = (tid == load_thread_num) ? load_data->Size() : from + load_pre_thead;
        // printf("thread-%d, from %lu to %lu\n", tid, from, to);

        load_set[tid] = new DataSet(load_data, from, to);
    }

    ///////////////////////////////////////////////////
    // load phase begin
    ///////////////////////////////////////////////////
    time_start = NowNanos();
    for (int tid = 0; tid < load_thread_num; tid++)
    {
        thread_vec.emplace_back(std::thread(TestFunc, load_set[tid], tid));
    }

    for (auto &f : thread_vec)
    {
        f.join();
    }
    printf("%d threads load time cost is %llu ns. error_count = %lld\n", load_thread_num, ElapsedNanos(time_start), total_error());

    for (int i = 0; i < load_thread_num; ++i)
    {
        delete load_set[i];
    }
    printf("create bnode = %llu, free bnode = %llu.\n", total_create_bnode(), total_free_bnode());

#ifdef TREE
    while (!CAS(&signal_do_recycle, false, true))
    {
    };
    while (signal_do_recycle == true)
    {
    };
    printf("log recycle for load phase! begin recycle\n");
#endif


#ifdef TREEFF
    while (!CAS(&signal_do_recycle, false, true))
    {
    };
    while (signal_do_recycle == true)
    {
    };
    printf("log recycle for load phase! begin recycle\n");
#endif

#ifdef DPTREE
    printf("wait for background..\n");
    while (bt->is_merging())
        ;
#endif

#ifdef PERF_LOG_MEMORY
    printf("peak log memory = %f MB\n", peak_log_memory / 1024.0 / 1024);
    printf("peak node memory = %f MB\n", peak_lnode_memory / 1024.0 / 1024);
    printf("end node memory = %f MB\n", total_lnode() * 256 / 1024.0 / 1024);
    printf("nvm log file need = %d\n", log_file_cnt);
#endif

    //空间开销统计
    printf("nvm space = %fMB\n", getNVMusage() / 1024.0 / 1024);
    // printf("nowRSS=%fMB  iniRSS=%fMB\n", getRSS()/1024.0/1024, ini_dram_space/1024.0/1024);
    printf("dram space (RSS) = %fMB\n", (getRSS() - ini_dram_space) / 1024.0 / 1024);

#ifdef DRAM_SPACE_TEST
    printf("dram space (non_lnode_space) = %fMB\n", dram_space / 1024.0 / 1024);
#endif
    printf("create bnode = %llu, free bnode = %llu.\n", total_create_bnode(), total_free_bnode());

    printf("load phast end !*******************\n");

#ifdef SPLIT_EVALUATION
    for (uint64_t tid = 0; tid < run_thread_num; tid++)
    {
        // hist_set_group[tid]->PrintResult();
        // printf("thread %d : insert elapsed time = %llu\n", tid, elapsed_time_group[tid]);
        hist_set_group[tid]->Clear();
    }
#endif

    ///////////////////////////////////////////////////
    // run phase begin
    ///////////////////////////////////////////////////
    clear_cache();
    thread_vec.clear();

    uint64_t run_pre_thead = run_data->Size() / run_thread_num;
    // printf("run_pre_thead: %lu * %d -> %zu\n", run_pre_thead, run_thread_num, run_data->Size());
    DataSet *run_set[run_thread_num];
    for (int tid = 0; tid < run_thread_num; tid++)
    {
        uint64_t from = tid * run_pre_thead;
        uint64_t to = (tid == run_thread_num) ? run_data->Size() : from + run_pre_thead;
        // printf("thread-%d, from %lu to %lu\n", tid, from, to);

        run_set[tid] = new DataSet(run_data, from, to);
    }

    time_start = NowNanos();
    for (int tid = 0; tid < run_thread_num; tid++)
    {
        thread_vec.emplace_back(std::thread(TestFunc, run_set[tid], tid));
    }

    for (auto &f : thread_vec)
    {
        f.join();
    }

    printf("%d threads run time cost is %llu ns. error_count = %lld\n", run_thread_num, ElapsedNanos(time_start), total_error());

    // for (int i = 0; i < load_thread_num; ++i)
    // {
    //     delete run_set[i];
    // }

#ifdef TREE
    signal_run_bgthread = false;
    bg_thread.get();
#endif

#ifdef DPTREE
    printf("wait for background..\n");
    while (bt->is_merging())
        ;
#endif


#ifdef TREEFF
    signal_run_bgthread = false;
    bg_thread.get();
#endif

#ifdef SPLIT_EVALUATION
    for (uint64_t tid = 0; tid < run_thread_num; tid++)
    {
        hist_set_group[tid]->PrintResult();
        if (tid == 47)
        {
            printf("span in NUMA\n");
        }
        // printf("thread %d : insert elapsed time = %llu\n", tid, elapsed_time_group[tid]);
    }
#endif

#ifdef PERF_LOG_MEMORY
    printf("peak log memory = %f MB\n", peak_log_memory / 1024.0 / 1024);
    printf("peak node memory = %f MB\n", peak_lnode_memory / 1024.0 / 1024);
    printf("end node memory = %f MB\n", total_lnode() * 256 / 1024.0 / 1024);
    printf("nvm log file need = %d\n", log_file_cnt);
#endif

    //空间开销统计
    printf("nvm space = %fMB\n", getNVMusage() / 1024.0 / 1024);
    // printf("nowRSS=%fMB  iniRSS=%fMB\n", getRSS()/1024.0/1024, ini_dram_space/1024.0/1024);
    printf("dram space (RSS) = %fMB\n", (getRSS() - ini_dram_space) / 1024.0 / 1024);

#ifdef DRAM_SPACE_TEST
    printf("dram space (non_lnode_space) = %fMB\n", dram_space / 1024.0 / 1024);
#endif

    printf("create bnode = %llu, free bnode = %llu.\n", total_create_bnode(), total_free_bnode());
}

int main(int argc, char const *argv[])
{
    if (argc != 4)
    {
        printf("usage: %s <load_file> <run_file> <thread_num>\n", argv[0]);
        return 0;
    }
    DataSet *load_data = new DataSet(argv[1]);
    DataSet *run_data = new DataSet(argv[2]);

    num_threads = atoi(argv[3]);

    /************************************ global variable*************************************/
    init_global_variable();
#ifndef NUMA_TEST
    openPmemobjPool("/mnt/pmem/sobtree/leafdata", 40ULL * 1024ULL * 1024ULL * 1024ULL, num_threads); //单节点
#else
    // numa 多节点
    openPmemobjPool("/mnt/pmem/sobtree/leafdata", "/pmem/sobtree/leafdata", 40ULL * 1024ULL * 1024ULL * 1024ULL);
#endif
    tree_init();

    RunYCSBBench(load_data, run_data, num_threads, num_threads);

#ifdef DPTREE
    tree_end();
#endif

    return 0;
}
