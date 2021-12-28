#set -e

source $MQ_ROOT/set_envs.sh

# Values for the first and second heatmap parameters (y and x axis).
HM_FST=( 1 2 4 8 16 32 64 128 256 512 1024 )
HM_SND=( 1 2 4 8 16 32 64 128 256 512 1024 )

# Supported algorithms
ALGOS=( "bfs" "sssp" "boruvka" "astar" )

clear_file () {
  echo "" > $1
}


mq=$1
C=$MQ_C
mq_c="${mq}_$C"

mq_name="Killme"
if [ $mq == "mqpp" ]; then
    mq_name="ProbProb"
  elif [ $mq == "mqpl" ]; then
    mq_name="ProbLocal"
  elif [ $mq == "mqlp" ]; then
    mq_name="LocalProb"
  elif [ $mq == "mqll" ]; then
    mq_name="LocalLocal"
fi

generate_heatmaps () {
  for p in "${HM_FST[@]}"
  do
    for ss in "${HM_SND[@]}"
      do
        name="${mq_c}_${p}_${ss}"
        echo "typedef MultiQueue${mq_name}<UpdateRequest, Comparer, $p, $ss, $C> $name;" >> $1
        echo "if (wl == \"$name\") RUN_WL($name);" >> $1
      done
  done
}

run_wl_n_times() {
  for run in $(seq 1 $2); do
    $MQ_ROOT/scripts/single_run/run_${algo}_${graph}.sh $1 $threads $3
  done
}

algo=$2
if ! [[ " ${ALGOS[@]} " =~ " ${algo} " ]]
then
  echo "Please, select supported algo: ${ALGOS[@]}"
  exit 1
fi

action=$3
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
  graph=$4
  threads=$5
  runs=$HM_RUNS
  for p in "${HM_FST[@]}"; do
    for ss in "${HM_SND[@]}"; do
        name="${mq_c}_${p}_${ss}"
        run_wl_n_times $name $runs "${algo}_${graph}_${mq}_$threads"
    done
  done
fi

