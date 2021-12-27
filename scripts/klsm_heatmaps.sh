set -e

source $MQ_ROOT/set_envs.sh

# Supported algorithms
ALGOS=( "bfs" "sssp" "boruvka" "astar" )
PARAMS=( 1 2 4 8 16 32 64 128 256 1024 2048 4096 16384 4194304 )

clear_file () {
  echo "" > $1
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
  echo "#include \"Galois/WorkList/kLSM_declarations.h\"" > $file
  $MQ_ROOT/scripts/build/build_${algo}.sh
elif [ $action == "run" ]; then
  graph=$3
  threads=$4
  min_p=$([ -z $5 ] && echo ${PARAMS[0]} || echo $5)
  max_p=$([ -z $6 ] && echo ${PARAMS[-1]} || echo $6)
  runs=$HM_RUNS
  for p in "${PARAMS[@]}"; do
    if [ $p -ge $min_p ] && [ $p -le $max_p ]; then
      run_wl_n_times "kLSM_${p}" $runs "${algo}_${graph}_klsm_$threads"
    fi
  done
fi

