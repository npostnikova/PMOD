#set -e

source $MQ_ROOT/set_envs.sh


# Save NUMA-related configuration.
echo "#define SOCKETS_NUM $NUMA_NODES" > $GALOIS_HOME/include/Galois/WorkList/MQOptimized/SOCKETS_NUM.h
echo "#define SOCKET_SIZE $SOCKET_SIZE" > $GALOIS_HOME/include/Galois/WorkList/MQOptimized/SOCKET_SIZE.h


wl=$1
echo "Running best numa for wl $wl"
hm_dir=$MQ_ROOT/experiments/$CPU/heatmaps
plt_dir=$MQ_ROOT/experiments/$CPU/plots
numa_path=$hm_dir/${wl}_heatmaps/numa
mkdir -p $numa_path
cd $numa_path
$PYTHON_EXPERIMENTS $MQ_ROOT/scripts/generate_numa.py $wl $HM_THREADS "$hm_dir/${wl}_heatmaps"
/bin/bash run_numa.sh
mkdir -p $plt_dir/${wl}_plots
cd $plt_dir/${wl}_plots
for algo in sssp bfs; do
  for graph in usa twi web west; do
    wl_name=$( $PYTHON_EXPERIMENTS $MQ_ROOT/scripts/find_best_wl.py "${numa_path}/${algo}_${graph}_${wl}_numa" )
    $MQ_ROOT/scripts/run_wl_n_times_all_threads.sh $algo $graph $wl_name $PLT_RUNS  "${wl}_numa"
  done
done

for algo in astar boruvka; do
  for graph in usa west; do
    wl_name=$( $PYTHON_EXPERIMENTS $MQ_ROOT/scripts/find_best_wl.py "${numa_path}/${algo}_${graph}_${wl}_numa" )
    $MQ_ROOT/scripts/run_wl_n_times_all_threads.sh $algo $graph $wl_name $HM_RUNS  "${wl}_numa"
  done
done