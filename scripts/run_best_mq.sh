#set -e

source $MQ_ROOT/set_envs.sh

t=$HM_THREADS
times=$PLT_RUNS
pyscript=$MQ_ROOT/scripts/find_best_wl.py
for mq in mqpl; do # mqpp mqpl mqlp mqll; do
#  dir=''
#  if [ $mq == "mqpp" ]; then
#    dir="probprob"
#  elif [ $mq == "mqpl" ]; then
#    dir="problocal"
#  elif [ $mq == "mqlp" ]; then
#    dir="localprob"
#  elif [ $mq == "mqll" ]; then
#    dir="locallocal"
#  fi
  hm_path=$MQ_ROOT/experiments/$CPU/heatmaps/${mq}_heatmaps/
  plt_dir=$MQ_ROOT/experiments/$CPU/plots/${mq}_plots/
  mkdir -p $plt_dir
  cd $plt_dir
  echo "Saving best ${mq} executions for all threads in $plt_dir"
  for algo in bfs sssp; do
    $MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq $algo build
    for graph in usa west twi web; do
        echo ">>>>"
        echo "$algo $graph"
        wl_name=$( $PYTHON_EXPERIMENTS $pyscript "$hm_path/${algo}_${graph}_${mq}_$t" )
        $MQ_ROOT/scripts/run_wl_n_times_all_threads.sh $algo $graph $wl_name $times $mq
    done
  done
  for algo in boruvka astar; do
    $MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq $algo build
    for graph in usa west; do
        echo ">>>>"
        echo "$algo $graph"
        wl_name=$( $PYTHON_EXPERIMENTS $pyscript "$hm_path/${algo}_${graph}_${mq}_$t" )
        $MQ_ROOT/scripts/run_wl_n_times_all_threads.sh $algo $graph $wl_name $times $mq
    done
  done
done