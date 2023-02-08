#!/bin/bash

num_keys=100000000
# num_keys=$2
rm -rf /mnt/pmem/sobtree/*
rm -rf ./mybin/*
wait

# CWARMING="-Wall" #显示所有警告
CWARMING="-w" #关闭所有警告
# CDEBUG="-DNDEBUG"  #注释->开启assert
CXXFLAG="-lpmem -lpmemobj  -mclwb -lpthread -mrtm -msse4.1 -mavx2 -mavx512f -mavx512bw -mclflushopt"
# CPPPATH="include/tools/mempool.c include/tools/minilog.c"  #v7
CPPPATH="include/tools/mempool.c include/tools/minilog.c include/tools/log.cpp"  #v8

threads=(47)
defines="-DDO_DELETE -DDO_UPDATE -DDO_SEARCH -DDO_SCAN"
defines=$defines" -DUNIFIED_NODE" #是否统一node大小，打开的话就是node全部只能存16个node.

echo $defines

if [ $1 = "lbtree" ] || [ $1 = "all" ]; then
echo "*******************normal test: lbtree*************************"
g++ -I include/multiThread/lbtree/lbtree-src -I include/multiThread/lbtree/common -I include $defines -DLBTREE -O3 -g $CDEBUG -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_normal_test_lbtree test/multiThread/normal_test.cpp include/multiThread/lbtree/common/tree.cc include/multiThread/lbtree/lbtree-src/lbtree.cc $CPPPATH $CXXFLAG
wait
for num_threads in ${threads[@]} 
do
numactl --membind=1 --cpunodebind=1 ./mybin/m_normal_test_lbtree $num_keys $num_threads $ssize
wait
rm -rf /mnt/pmem/sobtree/leafdata
wait
done
fi


if [ $1 = "fastfair" ] || [ $1 = "all" ]; then
echo "*******************normal test: fastfair*************************"
g++ -I include/multiThread/fast_fair -I include $defines -DFASTFAIR -O3 -g $CDEBUG  -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_normal_test_fastfair test/multiThread/normal_test.cpp $CPPPATH $CXXFLAG
wait
for num_threads in ${threads[@]} 
do
numactl --membind=1 --cpunodebind=1 ./mybin/m_normal_test_fastfair $num_keys $num_threads $ssize
wait
rm -rf /mnt/pmem/sobtree/leafdata
wait
done
fi

if [ $1 = "fptree" ] || [ $1 = "all" ]; then
echo "*******************normal test: fptree*************************"
g++ -I include/multiThread/fptree -I include $defines -DFPTREE -O3 -g $CDEBUG  -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_normal_test_fptree test/multiThread/normal_test.cpp include/multiThread/fptree/fptree.cpp $CPPPATH $CXXFLAG
wait
for num_threads in ${threads[@]} 
do
numactl --membind=1 --cpunodebind=1 ./mybin/m_normal_test_fptree $num_keys $num_threads $ssize
wait
rm -rf /mnt/pmem/sobtree/leafdata
wait
done
fi



if [ $1 = "sobtree" ] || [ $1 = "all" ]; then
echo "*******************normal test: sobtree*************************"
g++ -I include/multiThread/sobtree -I include $defines -DSOBTREE -O3 -g $CDEBUG -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_normal_test_sobtree test/multiThread/normal_test.cpp $CPPPATH $CXXFLAG
wait
for num_threads in ${threads[@]} 
do
numactl --membind=1 --cpunodebind=1 ./mybin/m_normal_test_sobtree $num_keys $num_threads
wait
rm -rf /mnt/pmem/sobtree/leafdata
wait
done
fi

