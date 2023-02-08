#!/bin/bash

# ycsb 的负载需要自己生成，现在只在紫金港服务器上生成了，其他服务器需要自己生成。

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

# numactl --membind=1 --cpunodebind=1 gdb ./mybin/m_ycsb_test_tree /home/zhenxin/git/YCSB/load.100M.txt /home/zhenxin/git/YCSB/workloada.run.50M.txt 1

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

if [ $1 = "sobtree" ] || [ $1 = "all" ]; then
g++ -I include/multiThread/sobtree -I include -DSOBTREE -O3 -g $CDEBUG -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_ycsb_test_sobtree test/multiThread/ycsb_test.cpp $CPPPATH $CXXFLAG
wait
for num_threads in ${threads[@]} 
do
for workload in sob_insert_only #sob_read_only sob_write_intensive sob_read_intensive sob_scan_insert
do
numactl --membind=1 --cpunodebind=1 ./mybin/m_ycsb_test_sobtree /home/zhenxin/git/YCSB/${workload}.load.txt /home/zhenxin/git/YCSB/${workload}.run.txt $num_threads
wait
rm -rf /mnt/pmem/sobtree/leafdata
wait
done
done
fi



# gdb --args numactl --membind=1 --cpunodebind=1 ./mybin/m_ycsb_test_dptree /home/zhenxin/git/YCSB/sob_insert_only.load.txt /home/zhenxin/git/YCSB/sob_insert_only.run.txt 1

# numactl --membind=1 --cpunodebind=1 ./mybin/m_ycsb_test_dptree /home/zhenxin/git/YCSB/sob_insert_only.load.txt /home/zhenxin/git/YCSB/sob_insert_only.run.txt 1
