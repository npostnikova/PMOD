set -e

source $MQ_ROOT/set_envs.sh

PROBS=("${HM_FST[@]}")
STEAL_SIZES=("${HM_SND[@]}")
# Supported algorithms
ALGOS=( "bfs" "sssp" "boruvka" "astar" )

clear_file () {
  echo "" > $1
}

generate_heatmaps () {
  for p in "${PROBS[@]}"
  do
    for ss in "${STEAL_SIZES[@]}"
      do
        name="slsmqhm_${p}_${ss}"
        echo "typedef SkipListSMQ<UpdateRequest, Comparer, $p, $ss> $name;" >> $1
        echo "if (wl == \"$name\") RUN_WL($name);" >> $1
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
  generate_heatmaps $file
  $MQ_ROOT/scripts/build/build_${algo}.sh
elif [ $action == "run" ]; then
  graph=$3
  threads=$4
  runs=$HM_RUNS
  for p in "${PROBS[@]}"; do
    for ss in "${STEAL_SIZES[@]}"; do
        run_wl_n_times "slsmqhm_${p}_${ss}" $runs "${algo}_${graph}_slsmq_$threads"
    done
  done
fi
