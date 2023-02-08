/*
search layer in dram. version 1:仅仅是有背景线程做分裂
*/
#pragma once
#include "util.h"

#define key_type_sob int64_t
#define NON_LEAF_KEY_NUM (14)
#define LEAF_KEY_NUM (14)
#define OPEN_RTM

class lnode;

POBJ_LAYOUT_BEGIN(btree);
POBJ_LAYOUT_TOID(btree, lnode);
POBJ_LAYOUT_END(btree);

#define bitScan(x) __builtin_ffs(x)
#define countBit(x) __builtin_popcount(x)
static int last_slot_in_line[LEAF_KEY_NUM];
static void initUseful(void)
{
    // line 0
    last_slot_in_line[0] = 2;
    last_slot_in_line[1] = 2;
    last_slot_in_line[2] = 2;

    // line 1
    last_slot_in_line[3] = 6;
    last_slot_in_line[4] = 6;
    last_slot_in_line[5] = 6;
    last_slot_in_line[6] = 6;

    // line 2
    last_slot_in_line[7] = 10;
    last_slot_in_line[8] = 10;
    last_slot_in_line[9] = 10;
    last_slot_in_line[10] = 10;

    // line 3
    last_slot_in_line[11] = 13;
    last_slot_in_line[12] = 13;
    last_slot_in_line[13] = 13;
}
void sfence()
{
    _mm_sfence();
}

#ifdef BACKGROUND
#define NUM_WORKERS 8
uint64_t CAPACITY = 1ULL * 1024ULL * 1024ULL;                                    // Queue capacity. Since there are more consumers than producers this doesn't have to be large
using Element = uint64_t;                                                        // Queue element type.
Element constexpr NIL = static_cast<Element>(0);                                 // Atomic elements require a special value that cannot be pushed/popped.
using Queue = atomic_queue::AtomicQueueB<Element, std::allocator<Element>, NIL>; // Use heap-allocated buffer.
#endif

/********************************************* The split information *******************************************/

#ifdef SPLIT_EVALUATION
// __thread HistogramSet *hist_set = new HistogramSet();
__thread HistogramSet *hist_set;
#endif

/********************************************* The structure *******************************************/
typedef struct entry
{
    key_type_sob k;
    uint64_t ch; //as a pointer
} entry;

/**
 *  inodeMeta: the 8B meta data in Non-leaf node
 */
typedef struct inodeMeta
{             /* 8B */
    int lock; /* lock bit for concurrency control */
    int num;  /* number of keys */
} inodeMeta;

/**
 * inode: non-leaf node
 *
 *   metadata (i.e. k(0))
 *
 *      k(1) .. k(NON_LEAF_KEY_NUM)
 *
 *   ch(0), ch(1) .. ch(NON_LEAF_KEY_NUM)
 */
class inode
{
public:
    entry ent[NON_LEAF_KEY_NUM + 1];

public:
    key_type_sob &k(int idx) { return ent[idx].k; }
    uint64_t &ch(int idx) { return ent[idx].ch; }
    int &num(void) { return ((inodeMeta *)ent)->num; }
    int &lock(void) { return ((inodeMeta *)ent)->lock; }

}; // inode

typedef struct lnodeMeta
{

    uint16_t bitmap : 14;
    uint16_t lock : 1;
    uint16_t alt : 1;
    unsigned char fgpt[LEAF_KEY_NUM]; /* fingerprints */
} lnodeMeta;

/**
 * lnode: leaf node
 *
 * We guarantee that each leaf must have >=1 key.
 */
class lnode
{
public:
    lnodeMeta meta;
    entry ent[LEAF_KEY_NUM];
    lnode *next[2];

public:
    key_type_sob &k(int idx) { return ent[idx].k; }
    uint64_t &ch(int idx) { return ent[idx].ch; }

    int num() { return countBit(meta.bitmap); }
    lnode *nextSibling() { return next[meta.alt]; }

    bool isFull(void) { return (meta.bitmap == 0x3fff); }

    void setBothWords(lnodeMeta *m)
    {
        memcpy(&meta, m, 16);
    }

    void setWord0(lnodeMeta *m)
    {
        memcpy(&meta, m, 8);
    }

}; // lnode

class tree
{
public:
    int root_level;
    inode *tree_root;
    lnode *first_lnode;
    tree();
    ~tree();
    void insert_inode(key_type_sob key, uint64_t val);
    uint64_t search_inode(key_type_sob key);

    void insert_lnode(key_type_sob key, uint64_t val);
    uint64_t search_lnode(key_type_sob key);

    void split(lnode *lp);
    void wait_for_split();
    void printinfo();
    void printinfo_leaf();
#ifdef BACKGROUND
public:
    std::thread consumers[NUM_WORKERS];
    Queue job_queue{CAPACITY};
#endif
};

inline lnode *alloc_lnode()
{
#ifndef MEMPOOL //alloc leaf node using PMDK
    TOID(lnode)
    leaf = TOID_NULL(lnode);
    POBJ_ZNEW(pop, &leaf, lnode);
    if (TOID_IS_NULL(leaf))
    {
        fprintf(stderr, "failed to create a btree leaf in nvmm.\n");
        exit(0);
    }
    return D_RW(leaf);
#else
    return (lnode *)nvmpool_alloc(sizeof(lnode));
#endif
}

// pos[] will contain sorted positions
inline void qsortBleaf(lnode *p, int start, int end, int pos[])
{
    if (start >= end)
        return;

    int pos_start = pos[start];
    key_type_sob key = p->k(pos_start); // pivot
    int l, r;

    l = start;
    r = end;
    while (l < r)
    {
        while ((l < r) && (p->k(pos[r]) > key))
            r--;
        if (l < r)
        {
            pos[l] = pos[r];
            l++;
        }
        while ((l < r) && (p->k(pos[l]) <= key))
            l++;
        if (l < r)
        {
            pos[r] = pos[l];
            r--;
        }
    }
    pos[l] = pos_start;
    qsortBleaf(p, start, l - 1, pos);
    qsortBleaf(p, l + 1, end, pos);
}

tree::tree()
{
    printf("create sobtree version 1!\n");
    initUseful();
    tree_root = new inode();
    first_lnode = alloc_lnode();
    tree_root->ch(0) = (uint64_t)first_lnode;
    root_level = 0; //bottom level is 0

#ifdef BACKGROUND
    for (int i = 0; i < NUM_WORKERS; i++)
    {
        consumers[i] = std::thread([&](int i)
                                   {
#ifdef MEMPOOL
                                       worker_id = num_threads + i;
#endif
                                       while (Element n = job_queue.pop()) // Stop when 0 is received.
                                       {
                                           split((lnode *)n);
                                           //    printf("A split (addr:%p) was completed by worker %d, size：%d\n", n, i, job_queue.was_size());
                                       }
                                   },
                                   i);
    }
#endif
}

#ifdef BACKGROUND
tree::~tree()
{
    for (auto &t : consumers)
        t.join();
}

void tree::wait_for_split()
{
    while (job_queue.was_size() != 0)
    {
    }
}
#endif

static inline unsigned char hashcode1B(key_type_sob x)
{
    x ^= x >> 32;
    x ^= x >> 16;
    x ^= x >> 8;
    return (unsigned char)(x & 0x0ffULL);
}

uint64_t tree::search_inode(key_type_sob key)
{
    inode *p;
    int i, t, m, b;
    key_type_sob r;

Again1:
#ifdef OPEN_RTM
    // 1. RTM begin
    if (_xbegin() != _XBEGIN_STARTED)
        goto Again1;
#endif
    sfence();
    // _mm_sfence();
    // 2. search nonleaf nodes
    p = tree_root;

    for (i = root_level; i >= 0; i--) //search from root to bottom.
    {

        // if the lock bit is set, abort
        if (p->lock())
        {
#ifdef OPEN_RTM
            _xabort(1);
#endif
            goto Again1;
        }

        // binary search to narrow down to at most 8 entries
        b = 1;
        t = p->num();
        while (b + 7 <= t)
        {
            m = (b + t) >> 1;
            r = key - p->k(m);
            if (r > 0)
                b = m + 1;
            else if (r < 0)
                t = m - 1;
            else
            {
                p = (inode *)p->ch(m);
                goto inner_done;
            }
        }

        // sequential search (which is slightly faster now)
        for (; b <= t; b++)
            if (key < p->k(b))
                break;
        p = (inode *)p->ch(b - 1);

    inner_done:;
    }

#ifdef OPEN_RTM
    // 4. RTM commit
    _xend();
#endif
    return (uint64_t)p;
}

void tree::insert_inode(key_type_sob key, uint64_t ptr)
{
    // record the path from root to leaf
    // parray[level] is a node on the path
    // child ppos[level] of parray[level] == parray[level-1]
    //
    inode *parray[32]; // 0 .. root_level will be used
    short ppos[32];    // 1 .. root_level will be used
    bool isfull[32];   // 0 .. root_level will be used

    // unsigned char key_hash = hashcode1B(key);
    volatile long long sum;

    /* Part 1. get the positions to insert the key */
    {
        inode *p;
        int i, t, m, b;
        key_type_sob r;

    Again2:
#ifdef OPEN_RTM
        // 1. RTM begin
        if (_xbegin() != _XBEGIN_STARTED)
        {

            goto Again2;
        }
#endif
        sfence();
        // _mm_sfence();
        // 2. search nonleaf nodes . 找到第0层的inode节点,不需要继续找。
        p = tree_root;
        for (i = root_level; i >= 0; i--)
        {

            // if the lock bit is set, abort
            if (p->lock())
            {
#ifdef OPEN_RTM
                _xabort(3);
#endif
                goto Again2;
            }

            parray[i] = p;
            isfull[i] = (p->num() == NON_LEAF_KEY_NUM);

            // binary search to narrow down to at most 8 entries
            b = 1;
            t = p->num();
            while (b + 7 <= t)
            {
                m = (b + t) >> 1;
                r = key - p->k(m);
                if (r > 0)
                    b = m + 1;
                else if (r < 0)
                    t = m - 1;
                else
                {
                    p = (inode *)p->ch(m);
                    ppos[i] = m;
                    goto inner_done;
                }
            }

            // sequential search (which is slightly faster now)
            for (; b <= t; b++)
                if (key < p->k(b))
                    break;

            p = (inode *)p->ch(b - 1);
            ppos[i] = b - 1;

        inner_done:;
        }

        // 4. set lock bits before exiting the RTM transaction

        // parray[0]->lock() = 1;

        for (i = 0; i <= root_level; i++)
        { //inode需要分裂，一层一层锁住
            parray[i]->lock() = 1;
            if (!isfull[i])
                break;
        }
        // if (isfull[0])
        // {
        //     for (i = 1; i <= root_level; i++)
        //     {
        //         parray[i]->lock() = 1;
        //         if (!isfull[i])
        //             break;
        //     }
        // }
#ifdef OPEN_RTM
        // 5. RTM commit
        _xend();
#endif

    } // end of Part 1

    /* Part 3. nonleaf node */
    {
        inode *p, *newp;
        int n, i, pos, r, lev, total_level;

#define LEFT_KEY_NUM ((NON_LEAF_KEY_NUM) / 2)
#define RIGHT_KEY_NUM ((NON_LEAF_KEY_NUM)-LEFT_KEY_NUM)

        total_level = root_level;
        lev = 0;

        while (lev <= total_level)
        {

            p = parray[lev];
            n = p->num();
            pos = ppos[lev] + 1; // the new child is ppos[lev]+1 >= 1

            /* if the non-leaf is not full, simply insert key ptr */

            if (n < NON_LEAF_KEY_NUM)
            {
                for (i = n; i >= pos; i--)
                    p->ent[i + 1] = p->ent[i];

                p->k(pos) = key;
                p->ch(pos) = ptr;
                p->num() = n + 1;
                sfence();

                // unlock after all changes are globally visible
                p->lock() = 0;
                return;
            }

            /* otherwise allocate a new non-leaf and redistribute the keys */
            //   newp = (inode *)mempool_alloc_node(NONLEAF_SIZE);
            newp = new inode();

            /* if key should be in the left node */
            if (pos <= LEFT_KEY_NUM)
            {
                for (r = RIGHT_KEY_NUM, i = NON_LEAF_KEY_NUM; r >= 0; r--, i--)
                {
                    newp->ent[r] = p->ent[i];
                }
                /* newp->key[0] actually is the key to be pushed up !!! */
                for (i = LEFT_KEY_NUM - 1; i >= pos; i--)
                    p->ent[i + 1] = p->ent[i];

                p->k(pos) = key;
                p->ch(pos) = ptr;
            }
            /* if key should be in the right node */
            else
            {
                for (r = RIGHT_KEY_NUM, i = NON_LEAF_KEY_NUM; i >= pos; i--, r--)
                {
                    newp->ent[r] = p->ent[i];
                }
                newp->k(r) = key;
                newp->ch(r) = ptr;
                r--;
                for (; r >= 0; r--, i--)
                {
                    newp->ent[r] = p->ent[i];
                }
            } /* end of else */

            key = newp->k(0);
            ptr = (uint64_t)newp;

            p->num() = LEFT_KEY_NUM;
            if (lev < total_level)
                p->lock() = 0; // do not clear lock bit of root
            newp->num() = RIGHT_KEY_NUM;
            newp->lock() = 0;

            lev++;
        } /* end of while loop */

        /* root was splitted !! add another level */
        //newp = (inode *)mempool_alloc_node(NONLEAF_SIZE);
        newp = new inode();

        newp->num() = 1;
        newp->lock() = 1;
        newp->ch(0) = (uint64_t)tree_root;
        newp->ch(1) = ptr;
        newp->k(1) = key;
        sfence(); // ensure new node is consistent

        inode *old_root = tree_root;
        root_level = lev;
        tree_root = newp;
        sfence(); // tree root change is globablly visible
                  // old root and new root are both locked

        // unlock old root
        old_root->lock() = 0;

        // unlock new root
        newp->lock() = 0;

        return;

#undef RIGHT_KEY_NUM
#undef LEFT_KEY_NUM
    }
}

uint64_t tree::search_lnode(key_type_sob key)
{
    inode *p;
    int i, t, m, b;
    key_type_sob r;

Again3:
#ifdef OPEN_RTM
    // 1. RTM begin
    if (_xbegin() != _XBEGIN_STARTED)
        goto Again3;
#endif
    sfence();
    // _mm_sfence();
    // 2. search nonleaf nodes
    p = tree_root;

    for (i = root_level; i >= 0; i--) //search from root to bottom.
    {

        // if the lock bit is set, abort
        if (p->lock())
        {
#ifdef OPEN_RTM
            _xabort(1);
#endif
            goto Again3;
        }

        // binary search to narrow down to at most 8 entries
        b = 1;
        t = p->num();
        while (b + 7 <= t)
        {
            m = (b + t) >> 1;
            r = key - p->k(m);
            if (r > 0)
                b = m + 1;
            else if (r < 0)
                t = m - 1;
            else
            {
                p = (inode *)p->ch(m);
                goto inner_done;
            }
        }

        // sequential search (which is slightly faster now)
        for (; b <= t; b++)
            if (key < p->k(b))
                break;
        p = (inode *)p->ch(b - 1);

    inner_done:;
    }

    // 3. search leaf node
    lnode *lp = (lnode *)p;

    // if the lock bit is set, abort
    if (lp->meta.lock)
    {
#ifdef OPEN_RTM
        _xabort(2);
#endif
        goto Again3;
    }

    unsigned char key_hash = hashcode1B(key);

    // SIMD comparison
    // a. set every byte to key_hash in a 16B register
    __m128i key_16B = _mm_set1_epi8((char)key_hash);

    // b. load meta into another 16B register
    __m128i fgpt_16B = _mm_load_si128((const __m128i *)lp);

    // c. compare them
    __m128i cmp_res = _mm_cmpeq_epi8(key_16B, fgpt_16B);

    // d. generate a mask
    unsigned int mask = (unsigned int)
        _mm_movemask_epi8(cmp_res); // 1: same; 0: diff

    // remove the lower 2 bits then AND bitmap
    mask = (mask >> 2) & ((unsigned int)(lp->meta.bitmap));

    // search every matching candidate
    int ret_pos = -1;
    while (mask)
    {
        int jj = bitScan(mask) - 1; // next candidate

        if (lp->k(jj) == key)
        { // found
            ret_pos = jj;
            break;
        }

        mask &= ~(0x1 << jj); // remove this bit
    }                         // end while

#ifdef OPEN_RTM
    // 4. RTM commit
    _xend();
#endif

    return ret_pos == -1 ? 0 : (uint64_t)lp->ch(ret_pos);
}

void tree::insert_lnode(key_type_sob key, uint64_t ptr)
{
#ifdef SPLIT_EVALUATION
    uint64_t time_start = NowNanos();
#endif
    lnode *lp;
    unsigned char key_hash;
    /* Part 1. get the positions to insert the key */
    {
        inode *p;
        int i, t, m, b;
        key_type_sob r;

    Again4:
#ifdef OPEN_RTM
        // 1. RTM begin
        if (_xbegin() != _XBEGIN_STARTED)
            goto Again4;
#endif
        sfence();
        // _mm_sfence();
        // 2. search nonleaf nodes
        p = tree_root;

        for (i = root_level; i >= 0; i--) //search from root to bottom.
        {

            // if the lock bit is set, abort
            if (p->lock())
            {
#ifdef OPEN_RTM
                _xabort(1);
#endif
                goto Again4;
            }

            // binary search to narrow down to at most 8 entries
            b = 1;
            t = p->num();
            while (b + 7 <= t)
            {
                m = (b + t) >> 1;
                r = key - p->k(m);
                if (r > 0)
                    b = m + 1;
                else if (r < 0)
                    t = m - 1;
                else
                {
                    p = (inode *)p->ch(m);
                    goto inner_done;
                }
            }

            // sequential search (which is slightly faster now)
            for (; b <= t; b++)
                if (key < p->k(b))
                    break;
            p = (inode *)p->ch(b - 1);

        inner_done:;
        }
        // 3. search leaf node
        lp = (lnode *)p;

        // if the lock bit is set, abort
        // if the lnode is full, abort
        if (lp->meta.lock || lp->isFull())
        {
#ifdef OPEN_RTM
            _xabort(4);
#endif
            goto Again4;
        }

        key_hash = hashcode1B(key);

        // SIMD comparison
        // a. set every byte to key_hash in a 16B register
        __m128i key_16B = _mm_set1_epi8((char)key_hash);

        // b. load meta into another 16B register
        __m128i fgpt_16B = _mm_load_si128((const __m128i *)lp);

        // c. compare them
        __m128i cmp_res = _mm_cmpeq_epi8(key_16B, fgpt_16B);

        // d. generate a mask
        unsigned int mask = (unsigned int)
            _mm_movemask_epi8(cmp_res); // 1: same; 0: diff

        // remove the lower 2 bits then AND bitmap
        mask = (mask >> 2) & ((unsigned int)(lp->meta.bitmap));

        // search every matching candidate
        while (mask)
        {
            int jj = bitScan(mask) - 1; // next candidate

            if (lp->k(jj) == key)
            { // found: do nothing, return
#ifdef OPEN_RTM
                _xend();
#endif
                return;
            }

            mask &= ~(0x1 << jj); // remove this bit
        }                         // end while

        // 4. set lock bits before exiting the RTM transaction

        lp->meta.lock = 1;

#ifdef OPEN_RTM
        // 5. RTM commit
        _xend();
#endif

    } // end of Part 1

#ifdef SPLIT_EVALUATION
    hist_set->Add(DO_RTM, ElapsedNanos(time_start));
    time_start = NowNanos();
#endif

    /* Part 2. leaf node */
    {
        assert(!lp->isFull());
        /* 1. leaf node is  definitely  not full */

        lnodeMeta meta; //temp meta
        memcpy(&meta, &lp->meta, sizeof(lnodeMeta));

        meta.lock = 0; // clear lock in temp meta

        // 1.1 get first empty slot
        uint16_t bitmap = meta.bitmap;
        int slot = bitScan(~bitmap) - 1;

        // 1.2 set leaf.entry[slot]= (k, v);
        // set fgpt, bitmap in meta
        lp->k(slot) = key;
        lp->ch(slot) = ptr;
        meta.fgpt[slot] = key_hash;
        bitmap |= (1 << slot);

        // 1.3 line 0: 0-2; line 1: 3-6; line 2: 7-10; line 3: 11-13
        // in line 0?
        if (slot < 3)
        {
            // 1.3.1 write word 0
            meta.bitmap = bitmap;
            lp->setWord0(&meta);

            // 1.3.2 flush
            //  clwb(lp); sfence();
            clflush(lp, CACHE_LINE_SIZE);
#ifdef SPLIT_EVALUATION
            hist_set->Add(DO_INSERT, ElapsedNanos(time_start));
#endif
            if (bitmap != 0x3fff)
            {
                // lp->meta.lock = 0; //不用分裂，解锁返回

                return;
            }
            else
            {
                // lp->meta.lock = 0;
#ifdef SPLIT_EVALUATION
                uint64_t time_start = NowNanos();
#endif
#ifndef BACKGROUND
                split(lp);
#else
                job_queue.push((uint64_t)lp);
#endif
#ifdef SPLIT_EVALUATION
                hist_set->Add(DO_SPLIT, ElapsedNanos(time_start));
#endif
                return;
            }
        }

        // 1.4 line 1--3
        else
        {
            int last_slot = last_slot_in_line[slot];
            int from = 0;
            for (int to = slot + 1; to <= last_slot; to++)
            {
                if ((bitmap & (1 << to)) == 0)
                {
                    // 1.4.1 for each empty slot in the line
                    // copy an entry from line 0
                    lp->ent[to] = lp->ent[from];
                    meta.fgpt[to] = meta.fgpt[from];
                    bitmap |= (1 << to);
                    bitmap &= ~(1 << from);
                    from++;
                }
            }

            // 1.4.2 flush the line containing slot
            clflush(&(lp->k(slot)), 8);
            // 1.4.3 change meta and flush line 0

            meta.bitmap = bitmap;
            lp->setBothWords(&meta);
            clflush(lp, CACHE_LINE_SIZE);

#ifdef SPLIT_EVALUATION
            hist_set->Add(DO_INSERT, ElapsedNanos(time_start));
#endif
            if (bitmap != 0x3fff)
            {
                // lp->meta.lock = 0; //不用分裂，解锁返回
                return;
            }
            else
            {
                // lp->meta.lock = 0;
#ifdef SPLIT_EVALUATION
                uint64_t time_start = NowNanos();
#endif
#ifndef BACKGROUND
                split(lp);
#else
                job_queue.push((uint64_t)lp);
#endif
#ifdef SPLIT_EVALUATION
                hist_set->Add(DO_SPLIT, ElapsedNanos(time_start));
#endif
                return;
            }
        }
    }
}

void tree::split(lnode *lp)
{
    /*
Again5:
#ifdef OPEN_RTM
    // 1. RTM begin (note :replace htm with cas?)
    if (_xbegin() != _XBEGIN_STARTED)
        goto Again5;
#endif

    // if the lock bit is set, abort
    if (lp->meta.lock)
    {
#ifdef OPEN_RTM
        _xabort(1);
#endif
        goto Again5;
    }

    lp->meta.lock = 1;

#ifdef OPEN_RTM
    // 5. RTM commit
    _xend();
#endif
*/
    lp->meta.lock = 1;
    sfence();
    /* 2. leaf is full, split */

    lnodeMeta meta; //temp meta
    memcpy(&meta, &lp->meta, sizeof(lnodeMeta));
    // 2.1 get sorted positions
    int sorted_pos[LEAF_KEY_NUM];
    for (int i = 0; i < LEAF_KEY_NUM; i++)
        sorted_pos[i] = i;
    qsortBleaf(lp, 0, LEAF_KEY_NUM - 1, sorted_pos);

    // 2.2 split point is the middle point
    int split = (LEAF_KEY_NUM / 2); // [0,..split-1] [split,LEAF_KEY_NUM-1]
    key_type_sob split_key = lp->k(sorted_pos[split]);

    // 2.3 create new node
    //lnode * newp = (lnode *)alloc_lnode(LEAF_SIZE);
    lnode *newp = (lnode *)alloc_lnode();
    // 2.4 move entries sorted_pos[split .. LEAF_KEY_NUM-1]
    uint16_t freed_slots = 0;
    for (int i = split; i < LEAF_KEY_NUM; i++)
    {
        newp->ent[i] = lp->ent[sorted_pos[i]];
        newp->meta.fgpt[i] = lp->meta.fgpt[sorted_pos[i]];

        // add to freed slots bitmap
        freed_slots |= (1 << sorted_pos[i]);
    }
    newp->meta.bitmap = (((1 << (LEAF_KEY_NUM - split)) - 1) << split);
    newp->meta.lock = 0;
    newp->meta.alt = 0;

    // remove freed slots from temp bitmap
    meta.bitmap &= ~freed_slots;

    newp->next[0] = lp->next[lp->meta.alt];
    lp->next[1 - lp->meta.alt] = newp;

    // set alt in temp bitmap
    meta.alt = 1 - lp->meta.alt;

    // 2.6 clwb newp, clwb lp line[3] and sfence
    clflush(newp, sizeof(lnode));
    clflush(&(lp->next[0]), 8); //flush pointer

    // 2.7 clwb lp and flush: NVM atomic write to switch alt and set bitmap
    // lp->setBothWords(&meta);
    lp->setWord0(&meta);
    clflush(lp, 8); //flush the first 8B in the header.

    /* (key, ptr) to be inserted in the parent non-leaf */
    insert_inode(split_key, (uint64_t)newp);

    lp->meta.lock = 0;
}

void tree::printinfo()
{
    std::queue<inode *> q;

    // inode *INL = new inode();
    // q.push(INL);
    q.push(tree_root);
    // q.push(INL);
    inode *temp;
    int level = 0;
    while (!q.empty())
    {
        temp = q.front();
        q.pop();
        // if ((uint64_t)temp == (uint64_t)INL) //换层了
        //     printf("\nlevel %d:", level++);
        // else
        if (temp->num() > 14 || temp->num() < 0)
            return;
        { //打印一个节点
            q.push((inode *)temp->ch(0));
            // printf("num = %llu ,", temp->num());
            for (int i = 1; i < temp->num() + 1; i++)
            {
                printf("%llu ", temp->k(i));
                q.push((inode *)temp->ch(i));
            }
            printf("||\n");
            // q.push(INL);
        }
    }
}

void tree::printinfo_leaf()
{
    lnode *curr = first_lnode;

    while (curr)
    {
        uint16_t bitmap = curr->meta.bitmap;
        for (int i = 0; i < LEAF_KEY_NUM; i++)
        {
            if (bitmap & 1ULL << i)
            {
                printf("%llu ", curr->ent[i].k);
            }
        }
        printf("||");
        curr = curr->next[curr->meta.alt];
    }
}