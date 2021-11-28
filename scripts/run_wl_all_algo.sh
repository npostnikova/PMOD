#set -e

wl=$1
threads=$2
times=$([ -z $3 ] && echo 3 || echo $3)
suff=$4
$MQ_ROOT/scripts/build/build.sh bfs 
#$MQ_ROOT/scripts/run_wl_n_times.sh bfs ctr $wl $threads $times $suff
$MQ_ROOT/scripts/run_wl_n_times.sh bfs usa $wl $threads $times $suff
$MQ_ROOT/scripts/run_wl_n_times.sh bfs west $wl $threads $times $suff
#$MQ_ROOT/scripts/run_wl_n_times.sh bfs lj $wl $threads $times $suff
$MQ_ROOT/scripts/run_wl_n_times.sh bfs twi $wl $threads $times $suff
$MQ_ROOT/scripts/run_wl_n_times.sh bfs web $wl $threads $times $suff
$MQ_ROOT/scripts/build/build.sh sssp 
#$MQ_ROOT/scripts/run_wl_n_times.sh sssp ctr $wl $threads $times $suff
$MQ_ROOT/scripts/run_wl_n_times.sh sssp usa $wl $threads $times $suff
$MQ_ROOT/scripts/run_wl_n_times.sh sssp west $wl $threads $times $suff
#$MQ_ROOT/scripts/run_wl_n_times.sh sssp lj $wl $threads $times $suff
$MQ_ROOT/scripts/run_wl_n_times.sh sssp twi $wl $threads $times $suff
$MQ_ROOT/scripts/run_wl_n_times.sh sssp web $wl $threads $times $suff
$MQ_ROOT/scripts/build/build.sh boruvka 
#$MQ_ROOT/scripts/run_wl_n_times.sh boruvka ctr $wl $threads $times $suff
$MQ_ROOT/scripts/run_wl_n_times.sh boruvka usa $wl $threads $times $suff
$MQ_ROOT/scripts/run_wl_n_times.sh boruvka west $wl $threads $times $suff
$MQ_ROOT/scripts/build/build.sh astar 
$MQ_ROOT/scripts/run_wl_n_times.sh astar west $wl $threads $times $suff
$MQ_ROOT/scripts/run_wl_n_times.sh astar usa $wl $threads $times $suff