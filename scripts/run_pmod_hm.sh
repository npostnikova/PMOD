#set -e

source $MQ_ROOT/set_envs.sh

threads=$1

$MQ_ROOT/scripts/pmod_heatmaps.sh bfs build
#$MQ_ROOT/scripts/pmod_heatmaps.sh bfs run ctr $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/pmod_heatmaps.sh bfs run usa $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/pmod_heatmaps.sh bfs run west $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
#$MQ_ROOT/scripts/pmod_heatmaps.sh bfs run lj $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/pmod_heatmaps.sh bfs run twi $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/pmod_heatmaps.sh bfs run web $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/pmod_heatmaps.sh sssp build
#$MQ_ROOT/scripts/pmod_heatmaps.sh sssp run ctr $threads 0 1024 $DELTA_LARGE_MIN $DELTA_LARGE_MAX
$MQ_ROOT/scripts/pmod_heatmaps.sh sssp run usa $threads 0 1024 $DELTA_LARGE_MIN $DELTA_LARGE_MAX
$MQ_ROOT/scripts/pmod_heatmaps.sh sssp run west $threads 0 1024 $DELTA_LARGE_MIN $DELTA_LARGE_MAX
#$MQ_ROOT/scripts/pmod_heatmaps.sh sssp run lj $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/pmod_heatmaps.sh sssp run twi $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/pmod_heatmaps.sh sssp run web $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/pmod_heatmaps.sh boruvka build
#$MQ_ROOT/scripts/pmod_heatmaps.sh boruvka run ctr $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/pmod_heatmaps.sh boruvka run usa $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/pmod_heatmaps.sh boruvka run west $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/pmod_heatmaps.sh astar build
$MQ_ROOT/scripts/pmod_heatmaps.sh astar run west $threads 0 1024 $DELTA_LARGE_MIN $DELTA_LARGE_MAX
$MQ_ROOT/scripts/pmod_heatmaps.sh astar run usa $threads 0 1024 $DELTA_LARGE_MIN $DELTA_LARGE_MAX
#$MQ_ROOT/scripts/pmod_heatmaps.sh astar run ctr $threads 0 1024 $DELTA_LARGE_MIN $DELTA_LARGE_MAX
