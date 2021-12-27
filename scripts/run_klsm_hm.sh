set -e

source $MQ_ROOT/set_envs.sh

threads=$1

$MQ_ROOT/scripts/klsm_heatmaps.sh bfs build
#$MQ_ROOT/scripts/klsm_heatmaps.sh bfs run ctr $threads 32 5000000
$MQ_ROOT/scripts/klsm_heatmaps.sh bfs run usa $threads 32 5000000
$MQ_ROOT/scripts/klsm_heatmaps.sh bfs run west $threads 32 5000000
#$MQ_ROOT/scripts/klsm_heatmaps.sh bfs run lj $threads 4000000 5000000
$MQ_ROOT/scripts/klsm_heatmaps.sh bfs run twi $threads 4000000 5000000
$MQ_ROOT/scripts/klsm_heatmaps.sh bfs run web $threads 4000000 5000000
$MQ_ROOT/scripts/klsm_heatmaps.sh sssp build
#$MQ_ROOT/scripts/klsm_heatmaps.sh sssp run ctr $threads 32 5000000
$MQ_ROOT/scripts/klsm_heatmaps.sh sssp run usa $threads 32 5000000
$MQ_ROOT/scripts/klsm_heatmaps.sh sssp run west $threads 32 5000000
#$MQ_ROOT/scripts/klsm_heatmaps.sh sssp run lj $threads 4000000 5000000
$MQ_ROOT/scripts/klsm_heatmaps.sh sssp run twi $threads 4000000 5000000
$MQ_ROOT/scripts/klsm_heatmaps.sh sssp run web $threads 4000000 5000000
$MQ_ROOT/scripts/klsm_heatmaps.sh boruvka build
#$MQ_ROOT/scripts/klsm_heatmaps.sh boruvka run ctr $threads 4000000 5000000
$MQ_ROOT/scripts/klsm_heatmaps.sh boruvka run usa $threads 4000000 5000000
$MQ_ROOT/scripts/klsm_heatmaps.sh boruvka run west $threads 4000000 5000000
$MQ_ROOT/scripts/klsm_heatmaps.sh astar build
$MQ_ROOT/scripts/klsm_heatmaps.sh astar run west $threads 32 5000000
$MQ_ROOT/scripts/klsm_heatmaps.sh astar run usa $threads 32 5000000
#$MQ_ROOT/scripts/klsm_heatmaps.sh astar run ctr $threads 32 5000000
