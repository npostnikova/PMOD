mqo=/nfs/scistore14/alistgrp/nkoval/PMOD/experiments/mq_optimized_hm/
t=256
pyscript=$MQ_ROOT/scripts/find_best_wl.py
for mq in mqpl; do
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
  cd $mqo/$dir
  python3.7 $mqo/generate_numa.py $mq
  cd numa
  mkdir -p best
  cd best
  for algo in bfs sssp; do
    for graph in usa west twi web; do
        echo ">>>>"
        echo "$algo $graph"
        wl_name=$( python3.7 $pyscript "../${algo}_${graph}_${mq}_numa" )
        $MQ_ROOT/scripts/run_wl_n_times_all_threads.sh $algo $graph $wl_name 5 $mq
    done
  done
  for algo in boruvka astar; do
    for graph in usa west; do
          echo ">>>>"
          echo "$algo $graph"
          wl_name=$( python3.7 $pyscript "../${algo}_${graph}_${mq}_numa" )
          $MQ_ROOT/scripts/run_wl_n_times_all_threads.sh $algo $graph $wl_name 5 $mq
      done
  done
done
