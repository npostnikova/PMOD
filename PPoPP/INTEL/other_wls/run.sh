for wl in spraylist heapswarm; do
  for t in 1 2 4 8 16 32 64 128; do  
    $MQ_ROOT/scripts/run_wl_all_algo.sh $wl $t 5 other
  done
done
