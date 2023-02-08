#!/bin/bash

num_keys=100000000 #50M
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

threads=(47)
# threads=(44)

if [ $# = 2 ]
then
    if [ $2 = "flush" ]; then
    skewness=(0.9)
    # defines=" -DZIP -DFLUSH_COUNT"
    defines=" -DZIP -DFLUSH_COUNT_USE_IPMCTL"
    elif [ $2 = "gc" ]; then
    skewness=(0.5 0.6 0.7 0.8 0.9 0.99)
    defines=" -DZIP -DTREE_NO_GC"
    fi
else
    # threads=(1 4 8 12 16 20 24 28 32 36 40 44 48)
    skewness=(0.9)
    defines=" -DZIP"
fi

definesdp=$defines
defines=$defines" -DUNIFIED_NODE"


if [ $1 = "tree" ] || [ $1 = "all" ]; then
echo "*******************zipfian test: tree*************************"
g++ -I include/multiThread/tree -I include $defines -DTREE -O3 -g $CDEBUG -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_zipfian_test_tree test/multiThread/zipfian_test.cpp $CPPPATH $CXXFLAG
wait
for zipfian_attri in ${skewness[@]}
do
    echo ${zipfian_attri}
    for num_threads in ${threads[@]} 
    do
    numactl --membind=1 --cpunodebind=1 ./mybin/m_zipfian_test_tree $num_keys $num_threads $zipfian_attri
    wait
    rm -rf /mnt/pmem/sobtree/leafdata
    wait
    done
done
fi


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



DPTREESRC="include/multiThread/dptree/src/"
CPPPATH_DPTREE="${DPTREESRC}art_idx.cpp ${DPTREESRC}ART.cpp ${DPTREESRC}bloom.c ${DPTREESRC}Epoche.cpp ${DPTREESRC}MurmurHash2.cpp  ${DPTREESRC}Tree.cpp ${DPTREESRC}dptree_util.cpp"

CPPLINK="-lpthread -ltbb"
if [ $1 = "dptree" ] || [ $1 = "all" ]; then
echo "*******************zipfian test: dptree*************************"
g++ -I include/multiThread/dptree/include -I include $definesdp -DDPTREE -O3 -g $CDEBUG -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_zipfian_test_dptree test/multiThread/zipfian_test.cpp $CPPPATH_DPTREE $CPPPATH $CXXFLAG $CPPLINK 
wait
for zipfian_attri in ${skewness[@]}
do
    echo ${zipfian_attri}
    for num_threads in ${threads[@]} 
    do
    numactl --membind=1 --cpunodebind=1 ./mybin/m_zipfian_test_dptree $num_keys $num_threads $zipfian_attri
    wait
    rm -rf /mnt/pmem/sobtree/leafdata
    wait
    done
done
fi


if [ $1 = "utree" ] || [ $1 = "all" ]; then
echo "*******************zipfian test: utree*************************"
g++ -I include/multiThread/utree -I include $defines -DUTREE -O3 -g $CDEBUG  -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_zipfian_test_utree test/multiThread/zipfian_test.cpp $CPPPATH $CXXFLAG
wait
for zipfian_attri in ${skewness[@]}
do
    echo ${zipfian_attri}
    for num_threads in ${threads[@]} 
    do
    numactl --membind=1 --cpunodebind=1 ./mybin/m_zipfian_test_utree $num_keys $num_threads $zipfian_attri
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

if [ $1 = "flatstore" ] || [ $1 = "all" ]; then
echo "******************* flatstore *************************"
g++ -I include/multiThread/flatstore -I include/multiThread/flatstore/common -I include $defines -DFLATSTORE -O3 -g $CDEBUG -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_zipfian_test_flatstore test/multiThread/zipfian_test.cpp include/multiThread/flatstore/common/tree.cc include/multiThread/flatstore/flatstore.cpp $CPPPATH $CXXFLAG
wait
for zipfian_attri in ${skewness[@]}
do
    echo ${zipfian_attri}
    for num_threads in ${threads[@]} 
    do
    numactl --membind=1 --cpunodebind=1 ./mybin/m_zipfian_test_flatstore $num_keys $num_threads $zipfian_attri
    wait
    rm -rf /mnt/pmem/sobtree/leafdata
    wait
    done
done
fi


# if [ $1 = "sobtree" ] || [ $1 = "all" ]; then
# g++ -I include/multiThread/sobtree -I include -DSOBTREE -O3 -g $CDEBUG -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_zipfian_test_sobtree test/multiThread/zipfian_test.cpp $CPPPATH $CXXFLAG
# wait
# for num_threads in 1 4 8 12 16 20 24 28 32 36 40 44 48 #1 4 8 12 16 20 24 28 32 36 40 #8 20 24 28 32 #1 2 4 8 16 30 32 64
# do
# numactl --membind=1 --cpunodebind=1 ./mybin/m_zipfian_test_sobtree $num_keys $num_threads $zipfian_attri
# wait
# rm -rf /mnt/pmem/sobtree/leafdata
# wait
# done
# fi


# numactl --membind=1 --cpunodebind=1 gdb ./mybin/m_zipfian_test_sobtree
# if [ $1 = "mpskiplist" ] || [ $1 = "all" ]; then
# g++ -I include/multiThread/mpskiplist -I include -DMPSKIPLIST -O3 -g $CDEBUG -m64  -fno-strict-aliasing -DINTEL $CWARMING -o mybin/m_zipfian_test_mpskiplist include/multiThread/mpskiplist/MPSkiplist_membm.cc test/multiThread/zipfian_test.cpp -lpmemobj -lpmem  -mclwb -lpthread -mrtm -msse4.1 -mavx2 -mavx512f -mavx512bw
# wait
# for num_threads in  1 4 8 12 16 20 24 28 32 #1 2 4 8 16 32
# do
# numactl --membind=1 --cpunodebind=1 ./mybin/m_zipfian_test_mpskiplist $num_keys $num_threads $zipfian_attri
# wait
# rm -rf /mnt/pmem/sobtree/leafdata
# wait
# done
# fi
