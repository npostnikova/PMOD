set -e

source $MQ_ROOT/set_envs.sh

t=$HM_THREADS
times=$PLT_RUNS
pyscript=$MQ_ROOT/scripts/find_best_wl.py
smq=$1 # smq or slsmq

hm_dir=$MQ_ROOT/experiments/$CPU/heatmaps/${smq}_heatmaps/
plt_dir=$MQ_ROOT/experiments/$CPU/plots/${smq}_plots/
$MQ_ROOT/scripts/${smq}_heatmaps.sh bfs build
$MQ_ROOT/scripts/${smq}_heatmaps.sh sssp build
$MQ_ROOT/scripts/${smq}_heatmaps.sh astar build
$MQ_ROOT/scripts/${smq}_heatmaps.sh boruvka build
mkdir -p plt_dir
cd plt_dir
echo "Writing all thread executions into $plt_dir"
for algo in bfs sssp; do
  for graph in usa west twi web; do
      echo ">>>>"
      echo "$algo $graph"
      wl_name=$( $PYTHON_EXPERIMENTS $pyscript "${hm_dir}/${algo}_${graph}_${smq}_$t" )
      $MQ_ROOT/scripts/run_wl_n_times_all_threads.sh $algo $graph $wl_name $times $smq
  done
done
for algo in boruvka astar; do
  for graph in usa west; do
      echo ">>>>"
      echo "$algo $graph"
      wl_name=$( $PYTHON_EXPERIMENTS $pyscript "${hm_dir}/${algo}_${graph}_${smq}_$t" )
      $MQ_ROOT/scripts/run_wl_n_times_all_threads.sh $algo $graph $wl_name $times $smq
  done
done