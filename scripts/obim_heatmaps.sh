#set -e

source $MQ_ROOT/set_envs.sh

# Supported algorithms
ALGOS=( "bfs" "sssp" "boruvka" "astar" )

threads=$4

clear_file () {
  echo "" > $1
}

generate_chunks () {
  for cs in "${CHUNK_SIZE[@]}"
  do
    echo "typedef dChunkedFIFO<$cs> dChunk_fifo_$cs;" >> $1
#    echo "typedef dChunkedLIFO<$cs> dChunk_lifo_$cs;" >> $1
  done
}

generate_obims () {
  for cs in "${CHUNK_SIZE[@]}"
  do
    for d in "${DELTAS[@]}"
      do
        for ct in "fifo" # "lifo"
        do
          obim_name="obim_${cs}_${d}_${ct}"
          indexer="ParameterizedUpdateRequestIndexer<UpdateRequest, $d>"
          echo "typedef OrderedByIntegerMetric<$indexer, dChunk_${ct}_${cs}, $2> $obim_name;" >> $1
          echo "if (wl == \"$obim_name\") RUN_WL($obim_name);" >> $1
        done
      done
  done
}

run_wls_default () {
  for cs in "${CHUNK_SIZE[@]}"; do
    for d in "${$1[@]}"; do
      for ct in "fifo"; do # "lifo"; do
        for wl in "obim"; do
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
  for run in $(seq 1 $2); do
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
  elif [ $algo == "sssp" ]; then
    generate_obims $file 10
  elif [ $algo == "astar" ]; then
    generate_obims $file 10
  elif [ $algo == "boruvka" ]; then
    generate_obims $file 10
  fi
  $MQ_ROOT/scripts/build/build_${algo}.sh
elif [ $action == "run" ]; then
  graph=$3
  runs=$HM_RUNS
  min_cs=$([ -z $5 ] && echo ${CHUNK_SIZE[0]} || echo $5)
  max_cs=$([ -z $6 ] && echo ${CHUNK_SIZE[-1]} || echo $6)
  min_d=$([ -z $7 ] && echo ${DELTAS[0]} || echo $7)
  max_d=$([ -z $8 ] && echo ${DELTAS[-1]} || echo $8)
  for cs in "${CHUNK_SIZE[@]}"; do
    echo $cs
    if [ $cs -ge $min_cs ] && [ $cs -le $max_cs ]; then
      for d in "${DELTAS[@]}"; do
        if [ $d -ge $min_d ] && [ $d -le $max_d ]; then
          run_wl_n_times "obim_${cs}_${d}_fifo" $runs "${algo}_${graph}_obim_$threads"
#          run_wl_n_times "obim_${cs}_${d}_lifo" $runs "${algo}_${graph}_obim_$threads"
        fi
      done
    fi
  done
fi
