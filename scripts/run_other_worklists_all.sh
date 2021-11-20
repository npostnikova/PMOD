source $MQ_ROOT/set_envs.sh

WLS=( swarm hmq$MQ_C heapswarm spraylist )
for wl in "${WLS[@]}" ; do
  for t in "${PLT_THREADS[@]}"; do
    $MQ_ROOT/scripts/run_wl_all_algo.sh $wl $t $PLT_RUNS other
  done
done
