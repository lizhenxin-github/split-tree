#pragma once

typedef struct log_entry_s log_entry_t;
typedef struct log_chunk_s log_chunk_t;
typedef struct log_s log_t;
typedef struct vlog_s vlog_t;
typedef struct log_group_s log_group_t;
typedef struct vlog_group_s vlog_group_t;
typedef struct free_log_chunks_s free_log_chunks_t;
typedef struct log_file_s log_file_t;


#define LOG_ENTRY_SIZE (sizeof(log_entry_t)) 
//4MB  
#define LOG_CHUNK_SIZE (4194304ULL)
//(4194304-8)/24
#define LOG_ENTRYS_PER_CHUNK 174762
//chunk size 的倍数
#define LOG_FILE_SIZE 10000
// #define LOG_FILE_SIZE 100

struct log_entry_s
{
   uint64_t key;
   uint64_t value;
   uint64_t timestamp;
//    uint64_t _;
};

struct log_chunk_s  //元数据和log数据放在一起
{
    log_chunk_t * next;
    log_entry_t log_entries[0]; 
};

//链表每次在尾部插入。vlog->now_chunk为尾部
struct log_s  
{
    log_chunk_t * head;  //头节点只分配一个指针大小用来存next
};

struct vlog_s  //存在dram中
{
    log_chunk_t * now_chunk;  //当前在用的chunk，即tail
    uint64_t tot_size;   //log总大小。用于判断log是否为空/是否需要触发gc
    uint64_t entry_cnt;  //当前chunk被使用了多少entry（=chunk内下标）
};

struct log_group_s  //每个线程有一个
{
    uint8_t alt; 
    log_t log[2];
};

struct vlog_group_s   //每个线程有一个
{
    uint8_t alt; 
    vlog_t vlog[2];
    uint64_t flushed_count[2];  //记录已经有多少log item是不需要的了
};


/*
目前：head存在dram中，中间的指针是log_chunk里的持久指针。优点是旧log被回收时，是整串塞进freelist里的，插入很快；缺点是取出chunk时要读持久指针

方法二：用simple list，所有指针存在dram。
*/
//全局chunk freelist，在头部插入删除（刚回收的chunk可能可以马上拿来用，也许有局部性）
struct free_log_chunks_s
{
    log_chunk_t * head;  //头节点只分配一个指针大小用来存next
    pthread_mutex_t lock;
};


/*
所有文件空间：
（2）在log freelist中
（3）在log list中

空间回收本身没问题。但是还是需要一个结构把文件串起来，用来删除文件本身。这个先不写
*/


void add_log(uint64_t key, uint64_t value);
void log_vlog_init(log_t &log, vlog_t &vlog, bool is_first);
uint64_t get_log_totsize();
uint64_t get_flush_totnum();

log_entry_t * add_log_for_flatstore(uint64_t key, uint64_t value);

/*
每个thread要有两个next指针
一个指向当前用的log list
一个指向gc时新的log list


chunk从系统拿来的时候要初始化为零
不初始化你不知道现在log尾部是哪里（恢复用）


thread local:
当前用哪个log  (gc)
当前用哪个log_chunk 

global:
free list
*/
