set -e

if [[ -z $1 || -z $2 ]]; then
  echo "Provide number of threads and python version"
  exit 1
fi
t=$1
pyscript=$MQ_ROOT/scripts/find_best_wl.py

cd $MQ_ROOT/experiments/slsmq_heatmaps
$MQ_ROOT/scripts/slsmq_heatmaps.sh $mq bfs build
$MQ_ROOT/scripts/slsmq_heatmaps.sh $mq sssp build
$MQ_ROOT/scripts/slsmq_heatmaps.sh $mq astar build
$MQ_ROOT/scripts/slsmq_heatmaps.sh $mq boruvka build
mkdir -p best
cd best
for algo in bfs sssp; do
  for graph in usa west twi web; do
      echo ">>>>"
      echo "$algo $graph"
      wl_name=$( python3.$2 $pyscript "../${algo}_${graph}_slsmq_$t" )
      $MQ_ROOT/scripts/run_wl_n_times_all_threads.sh $algo $graph $wl_name 5 slsmq
  done
done
for algo in boruvka astar; do
  for graph in usa west; do
    for wl in obim pmod; do
        echo ">>>>"
        echo "$algo $graph"
        wl_name=$( python3.$2 $pyscript "../${algo}_${graph}_slsmq_$t" )
        $MQ_ROOT/scripts/run_wl_n_times_all_threads.sh $algo $graph $wl_name 5 slsmq
    done
  done
done