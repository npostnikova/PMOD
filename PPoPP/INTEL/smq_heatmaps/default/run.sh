threads=$1

$MQ_ROOT/scripts/run_wl_n_times_all_threads.sh astar west smqhm_8_4 5 smqdef
exit
#$MQ_ROOT/scripts/smq_heatmaps.sh bfs build
$MQ_ROOT/scripts/smq_heatmaps.sh sssp build
$MQ_ROOT/scripts/run_wl_n_times_all_threads.sh sssp usa smqhm_8_4 5 smqdef
$MQ_ROOT/scripts/run_wl_n_times_all_threads.sh sssp west smqhm_8_4 5 smqdef
$MQ_ROOT/scripts/run_wl_n_times_all_threads.sh sssp twi smqhm_8_4 5 smqdef
$MQ_ROOT/scripts/run_wl_n_times_all_threads.sh sssp web smqhm_8_4 5 smqdef
$MQ_ROOT/scripts/smq_heatmaps.sh boruvka build
$MQ_ROOT/scripts/run_wl_n_times_all_threads.sh boruvka usa smqhm_8_4 5 smqdef
$MQ_ROOT/scripts/run_wl_n_times_all_threads.sh boruvka west smqhm_8_4 5 smqdef
$MQ_ROOT/scripts/smq_heatmaps.sh astar build
$MQ_ROOT/scripts/run_wl_n_times_all_threads.sh astar usa smqhm_8_4 5 smqdef
$MQ_ROOT/scripts/run_wl_n_times_all
