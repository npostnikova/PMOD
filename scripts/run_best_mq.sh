set -e

t=256
pyscript=$MQ_ROOT/scripts/find_best_wl.py
for mq in mqpp mqpl mqlp mqll; do
  dir=''
  if [ $mq == "mqpp" ]; then
    dir="probprob"
  elif [ $mq == "mqpl" ]; then
    dir="problocal"
  elif [ $mq == "mqlp" ]; then
    dir="localprob"
  elif [ $mq == "mqll" ]; then
    dir="locallocal"
  fi
  cd $MQ_ROOT/experiments/mq_optimized_heatmaps/$dir
  $MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq bfs build
  $MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq sssp build
  $MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq astar build
  $MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq boruvka build
  mkdir -p best
  cd best
  for algo in bfs sssp; do
    for graph in usa west twi web; do
        echo ">>>>"
        echo "$algo $graph"
        wl_name=$( python3.7 $pyscript "../${algo}_${graph}_${mq}_$t" )
        $MQ_ROOT/scripts/run_wl_n_times_all_threads.sh $algo $graph $wl_name 10 $mq
    done
  done
  for algo in boruvka astar; do
    for graph in usa west; do
      for wl in obim pmod; do
          echo ">>>>"
          echo "$algo $graph"
          wl_name=$( python3.7 $pyscript "../${algo}_${graph}_${mq}_$t" )
          $MQ_ROOT/scripts/run_wl_n_times_all_threads.sh $algo $graph $wl_name 10 $mq
      done
    done
  done
done