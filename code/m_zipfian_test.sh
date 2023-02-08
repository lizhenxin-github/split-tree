#!/bin/bash

num_keys=1000 #50M
# zipfian_attri=$2
# zipfian_attri=0.99
rm -rf /mnt/pmem/sobtree/*
rm -rf ./mybin/*
wait

# CWARMING="-Wall" #显示所有警告
CWARMING="-w" #关闭所有警告
CDEBUG="-DNDEBUG"  #不注释是关闭assert，注释是开启
CXXFLAG="-lpmem -lpmemobj  -mclwb -lpthread -mrtm -msse4.1 -mavx2 -mavx512f -mavx512bw -mclflushopt"
CPPPATH="include/tools/mempool.c include/tools/minilog.c include/tools/log.cpp"

threads=(20)
skewness=(0.9)
defines=" -DZIP"

if [ $1 = "lbtree" ] || [ $1 = "all" ]; then
echo "*******************zipfian test: lbtree*************************"
g++ -I include/multiThread/lbtree/lbtree-src -I include/multiThread/lbtree/common -I include $defines -DLBTREE -O3 -g $CDEBUG -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_zipfian_test_lbtree test/multiThread/zipfian_test.cpp include/multiThread/lbtree/common/tree.cc include/multiThread/lbtree/lbtree-src/lbtree.cc $CPPPATH $CXXFLAG
wait
for zipfian_attri in ${skewness[@]}
do
    echo ${zipfian_attri}
    for num_threads in ${threads[@]} 
    do
    numactl --membind=1 --cpunodebind=1 ./mybin/m_zipfian_test_lbtree $num_keys $num_threads $zipfian_attri
    wait
    rm -rf /mnt/pmem/sobtree/leafdata
    wait
    done
done
fi

if [ $1 = "fastfair" ] || [ $1 = "all" ]; then
echo "*******************zipfian test: fastfair*************************"
g++ -I include/multiThread/fast_fair -I include $defines -DFASTFAIR -O3 -g $CDEBUG  -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_zipfian_test_fastfair test/multiThread/zipfian_test.cpp $CPPPATH $CXXFLAG
wait
for zipfian_attri in ${skewness[@]}
do
    echo ${zipfian_attri}
    for num_threads in ${threads[@]} 
    do
    numactl --membind=1 --cpunodebind=1 ./mybin/m_zipfian_test_fastfair $num_keys $num_threads $zipfian_attri 
    wait
    rm -rf /mnt/pmem/sobtree/leafdata
    wait
    done
done
fi

if [ $1 = "fptree" ] || [ $1 = "all" ]; then
echo "*******************zipfian test: fptree*************************"
g++ -I include/multiThread/fptree -I include $defines -DFPTREE -O3 -g $CDEBUG  -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_zipfian_test_fptree test/multiThread/zipfian_test.cpp include/multiThread/fptree/fptree.cpp $CPPPATH $CXXFLAG
wait
for zipfian_attri in ${skewness[@]}
do
    echo ${zipfian_attri}
    for num_threads in ${threads[@]} 
    do
    numactl --membind=1 --cpunodebind=1 ./mybin/m_zipfian_test_fptree $num_keys $num_threads $zipfian_attri
    wait
    rm -rf /mnt/pmem/sobtree/leafdata
    wait
    done
done
fi

if [ $1 = "sobtree" ] || [ $1 = "all" ]; then
echo "*******************zipfian test:sobtree*************************"
g++ -I include/multiThread/sobtree -I include $defines -DSOBTREE -O3 -g $CDEBUG -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_zipfian_test_sobtree test/multiThread/zipfian_test.cpp $CPPPATH $CXXFLAG
wait
for zipfian_attri in ${skewness[@]}
do
    echo ${zipfian_attri}
    for num_threads in ${threads[@]} 
    do
    numactl --membind=1 --cpunodebind=1 ./mybin/m_zipfian_test_sobtree $num_keys $num_threads $zipfian_attri
    wait
    rm -rf /mnt/pmem/sobtree/leafdata
    wait
    done
done
fi


