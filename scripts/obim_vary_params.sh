set -e

lj=$MQ_ROOT/datasets/soc-LiveJournal1.bin
lj_tr=$MQ_ROOT/datasets/soc-LiveJournal1.bin_transpose
ctr=$MQ_ROOT/datasets/USA-road-dCTR.bin
usa=$MQ_ROOT/datasets/USA-road-dUSA.bin
usa_coord=$MQ_ROOT/datasets/USA-road-d.USA.co
web=$MQ_ROOT/datasets/GAP-web.bin
web_tr=$MQ_ROOT/datasets/GAP-web.bin_transpose
twi=$MQ_ROOT/datasets/GAP-twitter.bin
twi_tr=$MQ_ROOT/datasets/GAP-twitter.bin_transpose
west=$MQ_ROOT/datasets/USA-road-dW.bin
west_coord=$MQ_ROOT/datasets/USA-road-d.W.co


bfs=$GALOIS_HOME/build/apps/bfs/bfs
sssp=$GALOIS_HOME/build/apps/sssp/sssp
boruvka=$GALOIS_HOME/build/apps/boruvka/boruvka-merge
astar=$GALOIS_HOME/build/apps/astar/astar
pagerank=$GALOIS_HOME/build/apps/pagerank/pagerank

#if [ -z $1 ]
#then
#  echo "Please provide number of threads to run"
#  exit 1
#fi

# Generated deltas
DELTAS=( 0 1 2 4 6 8 12 14 16 18 )
# Generated chunk sizes
CHUNK_SIZE=( 1 2 4  8 16 32 64 128 256 512 )
# Supported algorithms
ALGOS=( "bfs" "sssp" "boruvka" "astar" )

clear_file () {
  echo "" > $1
}

generate_chunks () {
  for cs in "${CHUNK_SIZE[@]}"
  do
    echo "typedef dChunkedFIFO<$cs> dChunk_fifo_$cs;" >> $1
    echo "typedef dChunkedLIFO<$cs> dChunk_lifo_$cs;" >> $1
  done
}

generate_obims () {
  for cs in "${CHUNK_SIZE[@]}"
  do
    for d in "${DELTAS[@]}"
      do
        for ct in "fifo" "lifo"
        do
          obim_name="obim_${cs}_${d}_${ct}"
          indexer="ParameterizedUpdateRequestIndexer<UpdateRequest, $d>"
          echo "typedef OrderedByIntegerMetric<$indexer, dChunk_${ct}_${cs}, $2> $obim_name;" >> $1
          echo "if (wl == \"$obim_name\") RUN_WL($obim_name);" >> $1
        done
      done
  done
}

generate_pmods () {
  for cs in "${CHUNK_SIZE[@]}"
  do
    for d in "${DELTAS[@]}"
      do
        for ct in "fifo" "lifo"
        do
          pmod_name="pmod_${cs}_${d}_${ct}"
          indexer="ParameterizedUpdateRequestIndexer<UpdateRequest, $d>"
          echo "typedef AdaptiveOrderedByIntegerMetric<$indexer, dChunk_${ct}_$cs, $2, true, false, $cs> $pmod_name;" >> $1
          echo "if (wl == \"$pmod_name\") RUN_WL($pmod_name);" >> $1
        done
      done
  done
}

run_wls_default () {
  for cs in "${CHUNK_SIZE[@]}"; do
    for d in "${$1[@]}"; do
      for ct in "fifo" "lifo"; do
        for wl in "obim" "pmod"; do
           name="${wl}_${cs}_${d}_${ct}"
           for runs in $(seq 1 $runs); do
             ${!algo} ${!$2} -wl $name -t $THREADS -resultFile "${algo}_${graph}_${wl}_$THREADS"
           done
        done
      done
    done
  done
}

run_wl_n_times() {
  for run in $(seq 0 $2); do
    $MQ_ROOT/scripts/single_run/run_${algo}_${graph}.sh $1 $threads $3
  done
}


algo=$1
if ! [[ " ${ALGOS[@]} " =~ " ${algo} " ]]
then
  echo "Please, select supported algo: ${ALGOS[@]}"
  exit 1
fi

action=$2
ACTIONS=( build run )
if ! [[ " ${ACTIONS[@]} " =~ " ${action} " ]]
then
  echo "Please, select supported operation: build | run"
  exit 1
fi

if [ $action == "build" ]; then
  file="$GALOIS_HOME/apps/${algo}/Experiments.h"
  clear_file $file
  generate_chunks $file
  if [ $algo == "bfs" ]; then
    generate_obims $file 0
    generate_pmods $file 0
  elif [ $algo == "sssp" ]; then
    generate_obims $file 10
    generate_pmods $file 10
  elif [ $algo == "astar" ]; then
    generate_obims $file 10
    generate_pmods $file 10
  elif [ $algo == "boruvka" ]; then
    generate_obims $file 10
    generate_pmods $file 0
  fi
  $MQ_ROOT/scripts/build/build_${algo}.sh
elif [ $action == "run" ]; then
  graph=$3
  threads=$4
  min_cs=$([ -z $5 ] && echo ${CHUNK_SIZE[0]} || echo $5)
  max_cs=$([ -z $6 ] && echo ${CHUNK_SIZE[-1]} || echo $6)
  min_d=$([ -z $7 ] && echo ${DELTAS[0]} || echo $7)
  max_d=$([ -z $8 ] && echo ${DELTAS[-1]} || echo $8)
  echo $min_cs
  echo $max_cs
  runs=5
  for cs in "${CHUNK_SIZE[@]}"; do
    echo $cs
    if [ $cs -ge $min_cs ] && [ $cs -le $max_cs ]; then
      for d in "${DELTAS[@]}"; do
        if [ $d -ge $min_d ] && [ $d -le $max_d ]; then
          run_wl_n_times "obim_${cs}_${d}_fifo" $runs "${algo}_${graph}_obim_$threads"
          run_wl_n_times "obim_${cs}_${d}_lifo" $runs "${algo}_${graph}_obim_$threads"
          run_wl_n_times "pmod_${cs}_${d}_fifo" $runs "${algo}_${graph}_pmod_$threads"
          run_wl_n_times "pmod_${cs}_${d}_lifo" $runs "${algo}_${graph}_pmod_$threads"
        fi
      done
    fi
  done
fi

exit

runs=5
THREADS=$1

######## BFS
algo=bfs
file="$GALOIS_HOME/apps/${algo}/Experiments.h"

clear_file $file
generate_chunks $file
generate_obims $file 0
generate_pmods $file 0

DEL_RUN=( 0 1 2 4 6 )

run_wls_default $DEL_RUN "ctr"

exit 0



$MQ_ROOT/scripts/bfs_build.sh


exit 0

echo "" > $file
for i in $(seq 0 10)
do
  for j in $(seq 0 10)
  do
    prob=$(( 2 ** i ))
    size=$(( 2 ** j ))
    wl="mqplhm_${prob}_${size}"
    echo "typedef MultiQueueProbLocal<element_t, Comparer, ${prob}, ${size}, 2, priority_t> ${wl};" >> $file
    echo "if (worklistname == \"${wl}\")" >> $file
    echo "  Galois::for_each(WorkItem(source, 1), Process(graph), Galois::wl<${wl}>());" >> $file
  done
done

~/bfs_build.sh

for graph in ctr lj
do
  for i in $(seq 0 10)
  do
    for j in $(seq 0 10)
    do
      prob=$(( 2 ** i ))
      size=$(( 2 ** j ))
      wl="mqplhm_${prob}_${size}"
      for run in $(seq 1 $runs)
      do
          ${!algo} ${!graph} -wl $wl -t 96 -resultFile "${algo}_${graph}_hm"
      done
    done
  done
done

#for graph in twi
#do
#  for i in $(seq 0 10)
#  do
#    for j in $(seq 0 10)
#    do
#      prob=$(( 2 ** i ))
#      size=$(( 2 ** j ))
#      wl="mqplhm_${prob}_${size}"
#      for run in $(seq 1 $runs)
#      do
#          ${!algo} ${!graph} -wl $wl -t 96 -resultFile "${algo}_${graph}_hm" -startNode=14 -reportNode=15
#      done
#    done
#  done
#done

#for graph in web
#do
#  for i in $(seq 0 10)
#  do
#    for j in $(seq 0 10)
#    do
#      prob=$(( 2 ** i ))
#      size=$(( 2 ** j ))
#      wl="mqplhm_${prob}_${size}"
#      for run in $(seq 1 $runs)
#      do
#          ${!algo} ${!graph} -wl $wl -t 96 -resultFile "${algo}_${graph}_hm"  -startNode=5900 -reportNode=5901
#      done
#    done
#  done
#done

# Generate SMQ typedefs for SSSP
algo=sssp
file="$GALOIS_HOME/apps/${algo}/Experiments.h"
echo "" > $file
for i in $(seq 0 10)
do
  for j in $(seq 0 10)
  do
    prob=$(( 2 ** i ))
    size=$(( 2 ** j ))
    wl="mqplhm_${prob}_${size}"
    echo "typedef MultiQueueProbLocal<element_t, Comparer, ${prob}, ${size}, 2, priority_t> ${wl};" >> $file
    echo "if (worklistname == \"${wl}\")" >> $file
    echo "   Galois::for_each_local(initial, Process(this, graph), Galois::wl<${wl}>());" >> $file
  done
done

~/sssp_build.sh

for graph in ctr lj
do
  for i in $(seq 0 10)
  do
    for j in $(seq 0 10)
    do
      prob=$(( 2 ** i ))
      size=$(( 2 ** j ))
      wl="mqplhm_${prob}_${size}"
      for run in $(seq 1 $runs)
      do
          ${!algo} ${!graph} -wl $wl -t 96 -resultFile "${algo}_${graph}_hm"
      done
    done
  done
done

algo=astar
file="$GALOIS_HOME/apps/${algo}/Experiments.h"
echo "" > $file
for i in $(seq 0 10)
do
  for j in $(seq 0 10)
  do
    prob=$(( 2 ** i ))
    size=$(( 2 ** j ))
    wl="mqplhm_${prob}_${size}"
    echo "typedef MultiQueueProbLocal<element_t, Comparer, ${prob}, ${size}, 2, priority_t> ${wl};" >> $file
    echo "if (worklistname == \"${wl}\")" >> $file
    echo "   Galois::for_each_local(initial, Process(this, graph), Galois::wl<${wl}>());" >> $file
  done
done

~/astar_build.sh

for graph in west
do
  for i in $(seq 0 10)
  do
    for j in $(seq 0 10)
    do
      prob=$(( 2 ** i ))
      size=$(( 2 ** j ))
      wl="mqplhm_${prob}_${size}"
      for run in $(seq 1 $runs)
      do
        eval coord=\$$"${graph}_coord"
        ${!algo} ${!graph} -wl $wl -t 96 -resultFile "${algo}_${graph}_hm" -coordFilename $coord -delta 14 -startNode 1 -destNode 5639706  -reportNode 5639706
      done
    done
  done
done
#
algo=boruvka
file="$GALOIS_HOME/apps/${algo}/Experiments.h"
echo "" > $file
for i in $(seq 0 10)
do
  for j in $(seq 0 10)
  do
    prob=$(( 2 ** i ))
    size=$(( 2 ** j ))
    wl="mqplhm_${prob}_${size}"
    echo "typedef MultiQueueProbLocal<WorkItem, seq_gt, ${prob}, ${size}, 2, priority_t> ${wl};" >> $file
    echo "if (worklistname == \"${wl}\")" >> $file
    echo "   Galois::for_each_local(initial, process(), Galois::wl<${wl}>());" >> $file
  done
done

~/mst_build.sh

for graph in west
do
  for i in $(seq 0 10)
  do
    for j in $(seq 0 10)
    do
      prob=$(( 2 ** i ))
      size=$(( 2 ** j ))
      wl="mqplhm_${prob}_${size}"
      for run in $(seq 1 $runs)
      do
        eval coord=\$$"${graph}_coord"
        ${!algo} ${!graph} -wl $wl -t 96 -resultFile "${algo}_${graph}_hm"
      done
    done
  done
done

#algo=pagerank
#file="$GALOIS_HOME/apps/${algo}/Experiments.h"
#echo "" > $file
#for i in $(seq 0 10)
#do
#  for j in $(seq 0 10)
#  do
#    prob=$(( 2 ** i ))
#    size=$(( 2 ** j ))
#    wl="mqplhm_${prob}_${size}"
#    echo "typedef MultiQueueProbLocal<WorkItem, Comparer, ${prob}, ${size}, 2, priority_t> ${wl};" >> $file
#    echo "if (worklistname == \"${wl}\")" >> $file
#    echo "   Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn))," >> $file
#    echo "     boost::make_transform_iterator(graph.end(), std::ref(fn))," >> $file
#    echo "     Process(graph, tolerance, amp), Galois::wl<${wl}>());" >> $file
#  done
#done
#
##~/pr_build.sh
#
#for graph in lj
#do
#  for i in $(seq 0 10)
#  do
#    for j in $(seq 0 10)
#    do
#      prob=$(( 2 ** i ))
#      size=$(( 2 ** j ))
#      wl="mqplhm_${prob}_${size}"
#      for run in $(seq 1 $runs)
#      do
#        eval tr=\$$"${graph}_tr"
#        ${!algo} ${!graph} -wl $wl -t $t -resultFile "${algo}_${graph}_10" -graphTranspose $tr -amp 10 -tolerance 0.01
#     done
#    done
#  done
#done
#
