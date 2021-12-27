set -e

source $MQ_ROOT/set_envs.sh

threads=$1

$MQ_ROOT/scripts/klsm_params.sh bfs build
#$MQ_ROOT/scripts/klsm_params.sh bfs run ctr $threads 32 5000000
$MQ_ROOT/scripts/klsm_params.sh bfs run usa $threads 32 5000000
$MQ_ROOT/scripts/klsm_params.sh bfs run west $threads 32 5000000
#$MQ_ROOT/scripts/klsm_params.sh bfs run lj $threads 4000000 5000000
$MQ_ROOT/scripts/klsm_params.sh bfs run twi $threads 4000000 5000000
$MQ_ROOT/scripts/klsm_params.sh bfs run web $threads 4000000 5000000
$MQ_ROOT/scripts/klsm_params.sh sssp build
#$MQ_ROOT/scripts/klsm_params.sh sssp run ctr $threads 32 5000000
$MQ_ROOT/scripts/klsm_params.sh sssp run usa $threads 32 5000000
$MQ_ROOT/scripts/klsm_params.sh sssp run west $threads 32 5000000
#$MQ_ROOT/scripts/klsm_params.sh sssp run lj $threads 4000000 5000000
$MQ_ROOT/scripts/klsm_params.sh sssp run twi $threads 4000000 5000000
$MQ_ROOT/scripts/klsm_params.sh sssp run web $threads 4000000 5000000
$MQ_ROOT/scripts/klsm_params.sh boruvka build
#$MQ_ROOT/scripts/klsm_params.sh boruvka run ctr $threads 4000000 5000000
$MQ_ROOT/scripts/klsm_params.sh boruvka run usa $threads 4000000 5000000
$MQ_ROOT/scripts/klsm_params.sh boruvka run west $threads 4000000 5000000
$MQ_ROOT/scripts/klsm_params.sh astar build
$MQ_ROOT/scripts/klsm_params.sh astar run west $threads 32 5000000
$MQ_ROOT/scripts/klsm_params.sh astar run usa $threads 32 5000000
#$MQ_ROOT/scripts/klsm_params.sh astar run ctr $threads 32 5000000
