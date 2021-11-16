threads=128
MQS=( hmq1 hmq2 hmq3 hmq4 hmq5 )
times=5
suff=hmq
for mq in "${MQS[@]}"; do
  $MQ_ROOT/scripts/run_wl_all_algo.sh $mq $threads $times $suff
done

