set -e

source $MQ_ROOT/set_envs.sh

threads=$1

$MQ_ROOT/scripts/obim_vary_params.sh bfs build
#$MQ_ROOT/scripts/obim_vary_params.sh bfs run ctr $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/obim_vary_params.sh bfs run usa $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/obim_vary_params.sh bfs run west $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
#$MQ_ROOT/scripts/obim_vary_params.sh bfs run lj $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/obim_vary_params.sh bfs run twi $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/obim_vary_params.sh bfs run web $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/obim_vary_params.sh sssp build
#$MQ_ROOT/scripts/obim_vary_params.sh sssp run ctr $threads 0 1024 $DELTA_LARGE_MIN $DELTA_LARGE_MAX
$MQ_ROOT/scripts/obim_vary_params.sh sssp run usa $threads 0 1024 $DELTA_LARGE_MIN $DELTA_LARGE_MAX
$MQ_ROOT/scripts/obim_vary_params.sh sssp run west $threads 0 1024 $DELTA_LARGE_MIN $DELTA_LARGE_MAX
#$MQ_ROOT/scripts/obim_vary_params.sh sssp run lj $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/obim_vary_params.sh sssp run twi $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/obim_vary_params.sh sssp run web $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/obim_vary_params.sh boruvka build
#$MQ_ROOT/scripts/obim_vary_params.sh boruvka run ctr $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/obim_vary_params.sh boruvka run usa $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/obim_vary_params.sh boruvka run west $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/obim_vary_params.sh astar build
$MQ_ROOT/scripts/obim_vary_params.sh astar run west $threads 0 1024 $DELTA_LARGE_MIN $DELTA_LARGE_MAX
$MQ_ROOT/scripts/obim_vary_params.sh astar run usa $threads 0 1024 $DELTA_LARGE_MIN $DELTA_LARGE_MAX
#$MQ_ROOT/scripts/obim_vary_params.sh astar run ctr $threads 0 1024 $DELTA_LARGE_MIN $DELTA_LARGE_MAX
