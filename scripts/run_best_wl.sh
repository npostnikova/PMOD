#set -e

source $MQ_ROOT/set_envs.sh

t=$HM_THREADS
times=$PLT_RUNS
pyscript=$MQ_ROOT/scripts/find_best_wl.py
wl=$1

hm_dir=$MQ_ROOT/experiments/$CPU/heatmaps/${wl}_heatmaps/
plt_dir=$MQ_ROOT/experiments/$CPU/plots/${wl}_plots/
mkdir -p $plt_dir
cd $plt_dir
echo "Writing all thread executions into $plt_dir"
for algo in bfs sssp; do
  $MQ_ROOT/scripts/${wl}_heatmaps.sh $algo build
  for graph in usa west twi web; do
      echo ">>>>"
      echo "$algo $graph"
      wl_name=$( $PYTHON_EXPERIMENTS $pyscript "${hm_dir}/${algo}_${graph}_${wl}_$t" )
      $MQ_ROOT/scripts/run_wl_n_times_all_threads.sh $algo $graph $wl_name $times $wl
  done
done
for algo in boruvka astar; do
  $MQ_ROOT/scripts/${wl}_heatmaps.sh $algo build
  for graph in usa west; do
      echo ">>>>"
      echo "$algo $graph"
      wl_name=$( $PYTHON_EXPERIMENTS $pyscript "${hm_dir}/${algo}_${graph}_${wl}_$t" )
      $MQ_ROOT/scripts/run_wl_n_times_all_threads.sh $algo $graph $wl_name $times $wl
  done
done