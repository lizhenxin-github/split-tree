#!/bin/bash

rm -rf /mnt/pmem/sobtree/*
rm -rf ./mybin/*
wait

# CWARMING="-Wall" #显示所有警告
CWARMING="-w" #关闭所有警告
CDEBUG="-DNDEBUG"  #开启assert
CXXFLAG="-lpmem -lpmemobj  -mclwb -lpthread -mrtm -msse4.1 -mavx2 -mavx512f -mavx512bw -mclflushopt"
CPPPATH="include/tools/mempool.c include/tools/minilog.c include/tools/log.cpp"

defines=""
threads=(1)

if [ $# -ne 1 ]
then
    defines=""
    for para in $@
    do
        if [ $para = "flush" ]; then
        # defines=$defines" -DFLUSH_COUNT"
        defines=$defines" -DFLUSH_COUNT_USE_IPMCTL"
        fi

        if [ $para = "eADR" ]; then
        threads=(1 4 8 12 16 20 24 28 32 36 40 44 47)
        defines=$defines" -DeADR_TEST"
        fi
        
        if [ $para = "dramspace" ]; then
        defines=$defines" -DDRAM_SPACE_TEST" 
        fi

        if [ $para = "peaklog" ]; then
        defines=$defines" -DPERF_LOG_MEMORY" 
        fi
        
        if [ $para = "withgc" ]; then
        threads=(1 2 4 8 12 16 20 24 28 32 36 40 44 47)
        fi
        
        if [ $para = "nogc" ]; then
        threads=(47)
        defines=$defines" -DTREE_NO_GC"
        fi
        
        if [ $para = "nobuffer" ]; then
        defines=$defines" -DTREE_NO_BUFFER -DDO_UPDATE -DDO_DELETE"
        fi
        
        if [ $para = "noseleclog" ]; then
        defines=$defines" -DTREE_NO_SELECLOG"
        fi
        
        if [ $para = "nosearchcache" ]; then
        defines=$defines" -DTREE_NO_SEARCHCACHE" 
        fi

        if [ $para = "latency" ]; then
        defines=$defines" -DLATENCY_TEST -DDO_SEARCH" 
        fi
       
        if [ $para = "scan" ]; then
        scansize=(20 50 100 200)
        defines=$defines" -DDO_SCAN" 
        fi

        if [ $para = "recovery" ]; then
        defines=$defines" -DDO_RECOVERY" 
        fi

        if [ $para = "gcnaive" ]; then
        defines=$defines" -DGC_NAIVE" 
        fi

        if [ $para = "gcnaive2" ]; then
        defines=$defines" -DGC_NAIVE_2" 
        fi

        if [ $para = "perfgc" ]; then
        defines=$defines" -DPERF_GC" 
        fi

    done
else
    # threads=(1)
    threads=(36 47)
fi
buffer_pool_size=(0.1)
definesdp=$defines
defines=$defines" -DUNIFIED_NODE"

# numactl --membind=1 --cpunodebind=1 gdb ./mybin/m_ycsb_test_tree /home/zhenxin/git/YCSB/load.100M.txt /home/zhenxin/git/YCSB/workloada.run.50M.txt 1


if [ $1 = "tree" ] || [ $1 = "all" ]; then
echo "*******************ycsb test: tree*************************"
for bpoolsize in ${buffer_pool_size[@]}
do
g++ -I include/multiThread/tree -I include -DTREE $defines -DTHRESHOLD_OF_BUFFER_POOL=$bpoolsize -O3 -g $CDEBUG -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_ycsb_test_tree test/multiThread/ycsb_test.cpp $CPPPATH $CXXFLAG
wait
for num_threads in ${threads[@]} 
do
for workload in sob_insert_only #sob_read_only sob_write_intensive sob_read_intensive #sob_scan_insert
do
numactl --membind=1 --cpunodebind=1 ./mybin/m_ycsb_test_tree /home/zhenxin/git/YCSB/${workload}.load.txt /home/zhenxin/git/YCSB/${workload}.run.txt $num_threads
wait
rm -rf /mnt/pmem/sobtree/leafdata
wait
done
done
done
fi


if [ $1 = "treeff" ] || [ $1 = "all" ]; then
echo "*******************ycsb test: treeff*************************"
for bpoolsize in ${buffer_pool_size[@]}
do
g++ -I include/multiThread/tree_without_rtm -I include -DTREEFF $defines -DTHRESHOLD_OF_BUFFER_POOL=$bpoolsize -O3 -g $CDEBUG -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_ycsb_test_treeff test/multiThread/ycsb_test.cpp $CPPPATH $CXXFLAG
wait
for num_threads in ${threads[@]} 
do
for workload in sob_insert_only #sob_read_only sob_write_intensive sob_read_intensive #sob_scan_insert
do
numactl --membind=1 --cpunodebind=1 ./mybin/m_ycsb_test_treeff /home/zhenxin/git/YCSB/${workload}.load.txt /home/zhenxin/git/YCSB/${workload}.run.txt $num_threads
wait
rm -rf /mnt/pmem/sobtree/leafdata
wait
done
done
done
fi

if [ $1 = "lbtree" ] || [ $1 = "all" ]; then
echo "*******************ycsb test: lbtree*************************"
g++ -I include/multiThread/lbtree/lbtree-src -I include/multiThread/lbtree/common -I include -DLBTREE $defines -O3 -g $CDEBUG -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_ycsb_test_lbtree test/multiThread/ycsb_test.cpp include/multiThread/lbtree/common/tree.cc include/multiThread/lbtree/lbtree-src/lbtree.cc $CPPPATH $CXXFLAG
wait
for num_threads in ${threads[@]} 
do
for workload in sob_insert_only #sob_read_only sob_write_intensive sob_read_intensive sob_scan_insert
do
numactl --membind=1 --cpunodebind=1 ./mybin/m_ycsb_test_lbtree /home/zhenxin/git/YCSB/${workload}.load.txt /home/zhenxin/git/YCSB/${workload}.run.txt $num_threads
wait
rm -rf /mnt/pmem/sobtree/leafdata
wait
done
done
fi



DPTREESRC="include/multiThread/dptree/src/"
CPPPATH_DPTREE="${DPTREESRC}art_idx.cpp ${DPTREESRC}ART.cpp ${DPTREESRC}bloom.c ${DPTREESRC}Epoche.cpp ${DPTREESRC}MurmurHash2.cpp  ${DPTREESRC}Tree.cpp ${DPTREESRC}dptree_util.cpp"

CPPLINK="-lpthread -ltbb -std=c++11"
if [ $1 = "dptree" ] || [ $1 = "all" ]; then
echo "*******************ycsb test: dptree*************************"
g++ -I include/multiThread/dptree/include -I include -DDPTREE $definesdp -O3 -g $CDEBUG -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_ycsb_test_dptree test/multiThread/ycsb_test.cpp $CPPPATH_DPTREE $CPPPATH $CXXFLAG $CPPLINK 
wait
for num_threads in ${threads[@]} 
do
echo $num_threads
for workload in sob_insert_only #sob_read_only sob_write_intensive sob_read_intensive sob_scan_insert
do
echo ${workload}
numactl --membind=1 --cpunodebind=1 ./mybin/m_ycsb_test_dptree /home/zhenxin/git/YCSB/${workload}.load.txt /home/zhenxin/git/YCSB/${workload}.run.txt $num_threads
wait
rm -rf /mnt/pmem/sobtree/leafdata
wait
done
done
fi


if [ $1 = "utree" ] || [ $1 = "all" ]; then
echo "*******************ycsb test: utree*************************"
g++ -I include/multiThread/utree -I include -DUTREE $defines -O3 -g $CDEBUG  -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_ycsb_test_utree test/multiThread/ycsb_test.cpp $CPPPATH $CXXFLAG
wait
for num_threads in ${threads[@]} 
do
for workload in sob_insert_only #sob_read_only sob_write_intensive sob_read_intensive sob_scan_insert
do
numactl --membind=1 --cpunodebind=1 ./mybin/m_ycsb_test_utree /home/zhenxin/git/YCSB/${workload}.load.txt /home/zhenxin/git/YCSB/${workload}.run.txt $num_threads
wait
rm -rf /mnt/pmem/sobtree/leafdata
wait
done
done
fi



if [ $1 = "fastfair" ] || [ $1 = "all" ]; then
echo "*******************ycsb test: fastfair*************************"
g++ -I include/multiThread/fast_fair -I include -DFASTFAIR $defines -O3 -g $CDEBUG  -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_ycsb_test_fastfair test/multiThread/ycsb_test.cpp $CPPPATH $CXXFLAG
wait
for num_threads in ${threads[@]} 
do
for workload in sob_insert_only #sob_read_only sob_write_intensive sob_read_intensive sob_scan_insert
do
numactl --membind=1 --cpunodebind=1 ./mybin/m_ycsb_test_fastfair /home/zhenxin/git/YCSB/${workload}.load.txt /home/zhenxin/git/YCSB/${workload}.run.txt $num_threads
wait
rm -rf /mnt/pmem/sobtree/leafdata
wait
done
done
fi

if [ $1 = "fptree" ] || [ $1 = "all" ]; then
echo "*******************ycsb test: fptree*************************"
g++ -I include/multiThread/fptree -I include -DFPTREE $defines -O3 -g $CDEBUG  -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_ycsb_test_fptree test/multiThread/ycsb_test.cpp include/multiThread/fptree/fptree.cpp $CPPPATH $CXXFLAG
wait
for num_threads in ${threads[@]} 
do
for workload in sob_insert_only #sob_read_only sob_write_intensive sob_read_intensive sob_scan_insert
do
numactl --membind=1 --cpunodebind=1 ./mybin/m_ycsb_test_fptree /home/zhenxin/git/YCSB/${workload}.load.txt /home/zhenxin/git/YCSB/${workload}.run.txt $num_threads
wait
rm -rf /mnt/pmem/sobtree/leafdata
wait
done
done
fi

if [ $1 = "flatstore" ] || [ $1 = "all" ]; then
echo "******************* flatstore *************************"
g++ -I include/multiThread/flatstore -I include/multiThread/flatstore/common -I include -DFLATSTORE $defines -O3 -g $CDEBUG -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_ycsb_test_flatstore test/multiThread/ycsb_test.cpp include/multiThread/flatstore/common/tree.cc include/multiThread/flatstore/flatstore.cpp $CPPPATH $CXXFLAG
wait
for num_threads in ${threads[@]} 
do
for workload in sob_insert_only #sob_read_only sob_write_intensive sob_read_intensive sob_scan_insert
do
numactl --membind=1 --cpunodebind=1 ./mybin/m_ycsb_test_flatstore /home/zhenxin/git/YCSB/${workload}.load.txt /home/zhenxin/git/YCSB/${workload}.run.txt $num_threads
wait
rm -rf /mnt/pmem/sobtree/leafdata
wait
done
done
fi


# if [ $1 = "sobtree" ] || [ $1 = "all" ]; then
# g++ -I include/multiThread/sobtree -I include -DSOBTREE -O3 -g $CDEBUG -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_ycsb_test_sobtree test/multiThread/ycsb_test.cpp $CPPPATH $CXXFLAG
# wait
# for num_threads in ${threads[@]} 
# do
# for workload in sob_insert_only sob_read_only sob_write_intensive sob_read_intensive sob_scan_insert
# do
# numactl --membind=1 --cpunodebind=1 ./mybin/m_ycsb_test_sobtree /home/zhenxin/git/YCSB/${workload}.load.txt /home/zhenxin/git/YCSB/${workload}.run.txt $num_threads
# wait
# rm -rf /mnt/pmem/sobtree/leafdata
# wait
# done
# done
# fi



# gdb --args numactl --membind=1 --cpunodebind=1 ./mybin/m_ycsb_test_dptree /home/zhenxin/git/YCSB/sob_insert_only.load.txt /home/zhenxin/git/YCSB/sob_insert_only.run.txt 1

# numactl --membind=1 --cpunodebind=1 ./mybin/m_ycsb_test_dptree /home/zhenxin/git/YCSB/sob_insert_only.load.txt /home/zhenxin/git/YCSB/sob_insert_only.run.txt 1
