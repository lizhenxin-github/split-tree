// #define TREE
// #include <algorithm>

#pragma once

#define CHECK_RESULT

// #if 1
#if defined(FASTFAIR)
#include "btree.h"

btree *tree;
inline void tree_init()
{
    printf("init for multi-threads fast_fair!\n");
    tree = new btree();
};
inline void tree_end()
{
    delete tree;
};

inline void tree_insert(key_type_sob key)
{
#if defined(FLUSH_COUNT) || defined(INSERT_REPEAT_KEY)
    tree->btree_insert(key, (char *)key, true);
#else
    tree->btree_insert(key, (char *)key, false);
#endif
};
inline void tree_search(key_type_sob key)
{
    char *res = tree->btree_search(key);
#ifdef CHECK_RESULT
    if (unlikely(res == NULL))
    {
        count_error_search[thread_id]++;
    }
#endif
};

inline void tree_update(key_type_sob key)
{
    tree->btree_insert(key, (char *)key, true);
};

inline void tree_delete(key_type_sob key)
{
    tree->btree_delete(key);
};

inline void tree_scan(key_type_sob min_key, uint64_t length, std::vector<value_type_sob> &buf)
{
    int res = tree->btree_search_range_2(min_key, length, buf); // todo

    // std::sort(buf.begin(), buf.end()); //不用排序

    // for (int i = 0; i < res; i++)
    // {
    //     printf("%llu\t", buf[i]);
    // }
    // printf("\n");

#ifdef CHECK_RESULT
    if (unlikely(res == 0))
    {
        count_error_scan[thread_id]++;
        // printf("No key is larger than the min_key %d.\n", min_key);
        // exit(0);
    }
#endif
};

// #elif 1
#elif defined(FPTREE)
#include "fptree.h"

fptree_t *tree;

inline void tree_init()
{
    printf("init for multi-threads fptree!\n");
    tree = fptree_create();
};

inline void tree_insert(key_type_sob key)
{
#if defined(FLUSH_COUNT) || defined(INSERT_REPEAT_KEY)
    bool res = fptree_put(tree, key, (void *)key, true);
#else
    bool res = fptree_put(tree, key, (void *)key, false);
#endif

#ifdef CHECK_RESULT
    if (unlikely(res == false))
    {
        count_error_insert[thread_id]++;
        // printf("error: insert.\n");
        // count_error_search++;
        // exit(0);
    }
#endif
};

inline void tree_search(key_type_sob key) // bug
{
    bool res = fptree_get(tree, key);
#ifdef CHECK_RESULT
    if (unlikely(res == false))
    {
        count_error_search[thread_id]++;
        // printf("key = %llu\n", key);
        // printf("error: search.\n");
        // exit(0);
    }
#endif
};

inline void tree_update(key_type_sob key)
{
    bool res = fptree_put(tree, key, (void *)key, true);
#ifdef CHECK_RESULT
    if (unlikely(res == false))
    {
        count_error_update[thread_id]++;
        // printf("error: update.\n");
        // exit(0);
    }
#endif
};

inline void tree_delete(key_type_sob key)
{
    bool res = fptree_del(tree, key);
#ifdef CHECK_RESULT
    if (unlikely(res == false))
    {
        count_error_delete[thread_id]++;
        // exit(0);
    }
#endif
};

inline void tree_scan(key_type_sob min_key, uint64_t length, std::vector<value_type_sob> &buf)
{
    int res = fptree_scan(tree, min_key, length, buf);

    std::sort(buf.begin(), buf.end());

    // for (int i = 0; i < res; i++)
    // {
    //     printf("%llu\t", buf[i]);
    // }
    // printf("\n");

#ifdef CHECK_RESULT
    if (unlikely(res == 0))
    {
        count_error_scan[thread_id]++;
        // printf("error: scan.\n");
        // exit(0);
    }
#endif
};

// #elif 1
#elif defined(UTREE)
#include "utree.h"
btree *bt;

inline void tree_init()
{
    printf("init for multi-threads utree!\n");
    bt = new btree();
};

inline void tree_insert(key_type_sob key)
{
#if defined(FLUSH_COUNT) || defined(INSERT_REPEAT_KEY)
    bt->insert(key, (char *)key, true);
#else
    bt->insert(key, (char *)key, false);
#endif
};
inline void tree_search(key_type_sob key)
{
    bool res = bt->search(key);
#ifdef CHECK_RESULT
    if (unlikely(res == false))
    {
        count_error_search[thread_id]++;
    }
#endif
};

inline void tree_update(key_type_sob key)
{
    bt->insert(key, (char *)key, true);
};
inline void tree_delete(key_type_sob key)
{
    bt->remove(key);
};
inline void tree_scan(key_type_sob min_key, uint64_t length, std::vector<value_type_sob> &buf)
{
    int res = bt->scan(min_key, length, buf);

    // std::sort(buf.begin(), buf.end());  //不用排序

    // for (int i = 0; i < res; i++)
    // {
    //     printf("%llu\t", buf[i]);
    // }
    // printf("\n");

#ifdef CHECK_RESULT
    if (unlikely(res == 0))
    {
        count_error_scan[thread_id]++;
        // printf("error: scan.\n");
        // exit(0);
    }
#endif
};
// #elif 1
#elif defined(LBTREE)
#include "lbtree.h"
lbtree *bt;

inline void tree_init()
{
    printf("init for multi-threads lbtree!\n");
    bt = new lbtree(false);
};

inline void tree_insert(key_type_sob key)
{
#if defined(FLUSH_COUNT) || defined(INSERT_REPEAT_KEY)
    bt->insert(key, (char *)key, true);
#else
    bt->insert(key, (char *)key, false);
#endif
};

inline void tree_search(key_type_sob key)
{
    int index = 0;
    bleaf *lp = (bleaf *)bt->lookup(key, &index);
    value_type_sob res = lp->ch(index).value;
#ifdef CHECK_RESULT
    if (unlikely(res != key))
    {
        count_error_search[thread_id]++;
    }
// if (unlikely(lp->ch(index).value != key))
// {
// printf("error: search.res = %llu search.key =%llu, key = %llu\n", lp->ch(index).value, lp->k(index), key);
// exit(0);
// }
#endif
};

inline void tree_update(key_type_sob key)
{
    bt->insert(key, (char *)key, true);
};
inline void tree_delete(key_type_sob key)
{
    bt->del(key);
};

inline void tree_scan(key_type_sob min_key, uint64_t length, std::vector<value_type_sob> &buf)
{
    int res = bt->scan(min_key, length, buf);

    std::sort(buf.begin(), buf.end());

    // for (int i = 0; i < res; i++)
    // {
    //     printf("%llu\t", buf[i]);
    // }
    // printf("\n");

    if (unlikely(res == 0))
    {
        count_error_scan[thread_id]++;
        // printf("error: scan.\n");
        // exit(0);
    }
};

inline lbtree *tree_recovery(int num_threads)
{
    lbtree *lb_recovery = new lbtree(false);
    lb_recovery->recovery(*(bt->tree_meta->first_leaf), num_threads);
    return lb_recovery;
}

// #elif 1
#elif defined(SOBTREE)
#include "sobtree_v6.h"
tree *bt;

inline void tree_init()
{
    printf("init for multi-threads sobtree!\n");
    bt = new tree();
};

inline void tree_insert(key_type_sob key)
{
    // bt->insert_inode(key, key, true);
    // printf("key= %lu\n", key);
    bt->insert_lnode(key, key);
    // bt->insert_inode(key, key);
};

inline void tree_search(key_type_sob key)
{
    // printf("search key= %lu\n", key);
    uint64_t res = bt->search_lnode(key);
    // uint64_t res = bt->search_inode(key);
#ifdef CHECK_RESULT

    // if (unlikely(res != key))
    // {
    //     printf("error: search.\n");
    //     exit(0);
    // }
    if (unlikely(res != key))
    {
        printf("error: search.res = %llu search.key =%llu\n", res, key);
        // exit(0);
    }
#endif
};

inline void tree_update(key_type_sob key)
{
    bt->insert_lnode(key, key); // todo:
};
inline void tree_delete(key_type_sob key){
    // bt->del(key);  //todo
};
inline void tree_scan(key_type_sob min_key, uint64_t length, std::vector<value_type_sob> &buf)
{
    bt->scan(min_key, length, buf);
};
// #elif 1
#elif defined(MPSKIPLIST)
#include "MPSkiplist_membm.h"
MPSkipList *mp;

inline void tree_init()
{
    printf("init for multi-threads mpskiplist!\n");
    mp = init_skiplist();
};

inline void tree_insert(key_type_sob key)
{
    bool res = Insert(mp, key, key);
#ifdef CHECK_RESULT
    if (unlikely(res == false))
    {
        printf("error: insert.key = %llu \n", key);
        exit(0);
    }
#endif
};

inline void tree_search(key_type_sob key)
{
    uint64_t res = Search(mp, key);
#ifdef CHECK_RESULT
    // uint64_t res = bt->search_inode(key);
    // if (unlikely(res != key))
    // {
    //     printf("error: search.\n");
    //     exit(0);
    // }
    if (unlikely(res != key))
    {
        printf("error: search.res = %llu search.key =%llu\n", res, key);
        exit(0);
    }
#endif
};

inline void tree_update(key_type_sob key)
{
    uint64_t res = Update(mp, key, key + 1); // todo:
#ifdef CHECK_RESULT
    if (unlikely(res != key))
    {
        printf("error: search.res = %llu search.key =%llu\n", res, key);
        exit(0);
    }
#endif
};
inline void tree_delete(key_type_sob key){
    // bt->del(key);  //todo
};
inline void tree_scan(key_type_sob min_key, uint64_t length)
{
    assert("Scan op is not achievement!");
};
// #elif 1
#elif defined(TREE)
#include "tree_v11.h"
tree *bt;

inline void tree_init()
{
    printf("init for multi-threads tree!\n");
    bt = new tree();
};

inline void tree_insert(key_type_sob key)
{
    // bt->insert_inode(key, key, true);
    // printf("key= %lu\n", key);
#if defined(FLUSH_COUNT) || defined(INSERT_REPEAT_KEY)
    bt->insert_lnode(key, key, true);
#else
    bt->insert_lnode(key, key, false);
#endif
    // bt->insert_inode(key, key);
};

inline void tree_search(key_type_sob key)
{
    // printf("search key= %lu\n", key);
    uint64_t res = bt->search_lnode(key);
    // uint64_t res = bt->search_inode(key);
#ifdef CHECK_RESULT

    // if (unlikely(res != key))
    // {
    //     printf("error: search.\n");
    //     exit(0);
    // }
    if (unlikely(res != key))
    {
        count_error_search[thread_id]++;
        // exit(0);
    }
#endif
};

inline void tree_update(key_type_sob key)
{
    bt->insert_lnode(key, key, true);
};
inline void tree_delete(key_type_sob key)
{
    // bt->insert_inode(key, key, true);
    // printf("key= %lu\n", key);
    bt->insert_lnode(key, 0, true);
    // bt->insert_inode(key, key);
};

inline void tree_scan(key_type_sob min_key, uint64_t length, std::vector<value_type_sob> &buf)
{
    int res = bt->scan(min_key, length, buf);

    std::sort(buf.begin(), buf.end());

    // for (int i = 0; i < res; i++)
    // {
    //     printf("%llu\t", buf[i]);
    // }
    // printf("\n");

    if (unlikely(res == 0))
    {
        count_error_scan[thread_id]++;
        // printf("error: scan.\n");
        // exit(0);
    }
};

inline tree *tree_recovery(int log_groups_num, int num_work_threads)
{
    tree *tr_recovery = new tree(true);
    // tr_recovery->recovery(bt->first_lnode, log_groups, num_work_threads, log_groups_num);
    return tr_recovery;
}

inline void tree_recycle()
{
    // bt->recycle_bottom_naive();
    bt->clean_bottom();
}

// #elif 1
#elif defined(DPTREE)
#include "concur_dptree.hpp"
dptree::concur_dptree<key_type_sob, value_type_sob> *bt;

inline void tree_init()
{
    printf("init for multi-threads dptree!\n");
    bt = new dptree::concur_dptree<key_type_sob, value_type_sob>();
};

inline void tree_end()
{
    delete bt;
}

inline void tree_insert(key_type_sob key)
{
    // bt->insert_inode(key, key, true);
    // printf("key= %lu\n", key);
    bt->insert(key, key);
    // bt->insert_inode(key, key);
};

inline void tree_search(key_type_sob key)
{
    // printf("search key= %lu\n", key);
    bool res = bt->lookup(key, key);
    // uint64_t res = bt->search_inode(key);
#ifdef CHECK_RESULT

    // if (unlikely(res != key))
    // {
    //     printf("error: search.\n");
    //     exit(0);
    // }
    if (unlikely(res == false))
    {
        count_error_search[thread_id]++;
        // printf("error: search.res = %llu search.key =%llu\n", res, key);
        // exit(0);
    }
#endif
};

inline void tree_update(key_type_sob key)
{
    bt->upsert(key, key); //
};
inline void tree_delete(key_type_sob key)
{
    // bt->insert_inode(key, key, true);
    // printf("key= %lu\n", key);
    // assert("delete op is not achievement!");
    // bt->insert_inode(key, key);
    bt->upsert(key, 1); //
};
inline void tree_scan(key_type_sob min_key, uint64_t length, std::vector<value_type_sob> &buf)
{
    int res = bt->lookup_range(min_key, length, buf);

    // std::sort(buf.begin(), buf.end());   //不用排序

    // for (int i = 0; i < res; i++)
    // {
    //     printf("%llu\t", buf[i]);
    // }
    // printf("\n\n\n\n\n");

    if (unlikely(res == 0))
    {
        count_error_scan[thread_id]++;
        // printf("error: scan.\n");
        // exit(0);
    }
};

    // #elif defined(DPTREENEW)
    // #include "concur_dptree.hpp"
    // dptree::concur_dptree<key_type_sob, value_type_sob> *bt;
    // int parallel_merge_worker_num = 0;

    // inline void tree_init()
    // {
    //     printf("init for multi-threads dptree!\n");
    //     bt = new dptree::concur_dptree<key_type_sob, value_type_sob>();
    //     parallel_merge_worker_num = num_threads;
    // };

    // inline void tree_insert(key_type_sob key)
    // {
    //     // bt->insert_inode(key, key, true);
    //     // printf("key= %lu\n", key);
    //     bt->insert(key, key);
    //     // bt->insert_inode(key, key);
    // };

    // inline void tree_search(key_type_sob key)
    // {
    //     // printf("search key= %lu\n", key);
    //     bool res = bt->lookup(key, key);
    //     // uint64_t res = bt->search_inode(key);
    // #ifdef CHECK_RESULT

    //     // if (unlikely(res != key))
    //     // {
    //     //     printf("error: search.\n");
    //     //     exit(0);
    //     // }
    //     if (unlikely(res == false))
    //     {
    //         count_error_search[thread_id]++;
    //         // printf("error: search.res = %llu search.key =%llu\n", res, key);
    //         // exit(0);
    //     }
    // #endif
    // };

    // inline void tree_update(key_type_sob key)
    // {
    //     bt->upsert(key, key); //
    // };
    // inline void tree_delete(key_type_sob key)
    // {
    //     // bt->insert_inode(key, key, true);
    //     // printf("key= %lu\n", key);
    //     // assert("delete op is not achievement!");
    //     // bt->insert_inode(key, key);
    //     bt->upsert(key, 1); //
    // };
    // inline void tree_scan(key_type_sob min_key, uint64_t length, std::vector<value_type_sob> &buf)
    // {
    //     int res = bt->lookup_range(min_key, length, buf);

    //     // std::sort(buf.begin(), buf.end());   //不用排序

    //     // for (int i = 0; i < res; i++)
    //     // {
    //     //     printf("%llu\t", buf[i]);
    //     // }
    //     // printf("\n\n\n\n\n");

    //     if (unlikely(res == 0))
    //     {
    //         count_error_scan[thread_id]++;
    //         // printf("error: scan.\n");
    //         // exit(0);
    //     }
    // };

#elif defined(FLATSTORE)
#include "flatstore.h"
flatstore *bt;

inline void tree_init()
{
    printf("init for multi-threads flatstore!\n");
    bt = new flatstore(false);
};

inline void tree_insert(key_type_sob key)
{
    bt->insert(key, (char *)key);
};

inline void tree_search(key_type_sob key)
{
    // int index = 0;
    value_type_sob res = bt->lookup(key);
#ifdef CHECK_RESULT
    if (unlikely(res == 0))
    {
        count_error_search[thread_id]++;
    }
    if (unlikely(res != key))
    {
        printf("error: search.res = %llu key = %llu\n", res, key);
        exit(0);
    }
#endif
};

inline void tree_update(key_type_sob key)
{
    bt->insert(key, (char *)key);
};
inline void tree_delete(key_type_sob key)
{
    bt->del(key);
};

inline void tree_scan(key_type_sob min_key, uint64_t length, std::vector<value_type_sob> &buf)
{
    int res = bt->scan(min_key, length, buf);

    std::sort(buf.begin(), buf.end());
    // printf("%d\n", res);
    // for (int i = 0; i < res; i++)
    // {
    //     printf("%llu\t", buf[i]);
    // }
    // printf("\n");

    if (unlikely(res == 0))
    {
        count_error_scan[thread_id]++;
        // printf("error: scan.\n");
        // exit(0);
    }
};

// #elif 1
#elif defined(TREEFF)
#include "treeff_v2.h"

btree *tree;
inline void tree_init()
{
    printf("init for multi-threads fast_fair!\n");
    tree = new btree();
};
inline void tree_end()
{
    delete tree;
};

inline void tree_insert(key_type_sob key)
{
#if defined(FLUSH_COUNT) || defined(INSERT_REPEAT_KEY)
    tree->insert(key, (char *)key, true);
#else
    tree->insert(key, (char *)key, false);
#endif
};
inline void tree_search(key_type_sob key)
{
    char *res = tree->search(key);
#ifdef CHECK_RESULT
    if (unlikely(res != (char *)key))
    {
        // tree->printAll();

        // printf("search not found: %llu, ret = %llu \n", key,uint64_t(res));

        // tree->printinfo_leaf();
        // printf("\n\n\n\n");
        // _mm_mfence();
        // exit(0);
        count_error_search[thread_id]++;
    }
#endif
};

inline void tree_update(key_type_sob key)
{
    tree->insert(key, (char *)key, true);
};

inline void tree_delete(key_type_sob key)
{
    tree->insert(key, (char *)0, true);
};

inline void tree_scan(key_type_sob min_key, uint64_t length, std::vector<value_type_sob> &buf)
{
    int res = tree->btree_search_range(min_key, length, buf); 

    std::sort(buf.begin(), buf.end());
    // for (int i = 0; i < res; i++)
    // {
    //     printf("%llu\t", buf[i]);
    // }
    // printf("\n\n\n");

#ifdef CHECK_RESULT
    if (unlikely(res == 0))
    {
        count_error_scan[thread_id]++;
        // printf("No key is larger than the min_key %d.\n", min_key);
        // exit(0);
    }
#endif
};

inline void tree_recycle()
{
    // bt->recycle_bottom_naive();
    tree->clean_bottom();
}

#elif defined(PACTREE)

#include "pactree_wrapper.h"
pactree_wrapper * pt_wrapper;
thread_local key_type_sob global_key_ptr;  //TODO:改成thread local
thread_local value_type_sob global_value_ptr;

inline void tree_init()
{
    printf("init for multi-threads pactree!\n");
    // pt_wrapper = new pactree_wrapper();
    tree_options_t tree_opt;
    tree_opt.pool_path = "/pmem/sobtree/";  // node 0
    pt_wrapper = reinterpret_cast<pactree_wrapper *>(create_tree(tree_opt));
};


inline void tree_end()
{
    delete pt_wrapper;
}

inline void tree_insert(key_type_sob key)
{
    global_key_ptr = key;
    global_value_ptr = key;
    pt_wrapper->insert(reinterpret_cast<char*>(&global_key_ptr), 8, reinterpret_cast<char*>(&global_value_ptr), 8);
};

inline void tree_search(key_type_sob key)
{
    global_key_ptr = key;
    pt_wrapper->find(reinterpret_cast<char*>(&global_key_ptr), 8, reinterpret_cast<char*>(&global_value_ptr));
};

inline void tree_update(key_type_sob key)
{
    global_key_ptr = key;
    global_value_ptr = key;
    pt_wrapper->update(reinterpret_cast<char*>(&global_key_ptr), 8, reinterpret_cast<char*>(&global_value_ptr), 8);
};

inline void tree_delete(key_type_sob key)
{
    global_key_ptr = key;
    pt_wrapper->remove(reinterpret_cast<char*>(&global_key_ptr), 8);
};

inline void tree_scan(key_type_sob min_key, uint64_t length, std::vector<value_type_sob> &buf)
{
    global_key_ptr = min_key;
    // std::vector<char *> buf2;
    char * buf2;
    pt_wrapper->scan(reinterpret_cast<char*>(&global_key_ptr), 8, length, buf2);

    // sort in pactree_wrapper.cpp
};

inline void tree_get_memory_footprint()  // only for pactree
{
    pt_wrapper->get_memory_footprint();
};

#endif

