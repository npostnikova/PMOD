threads=128
times=3
suff=klsm
for p in {1..12} 16384 4194304; do
  wl="kLSM_$p"
  $MQ_ROOT/scripts/run_wl_all_algo.sh $wl $threads $times $suff
done

