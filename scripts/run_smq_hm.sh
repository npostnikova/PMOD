set -e

threads=$1

$MQ_ROOT/scripts/smq_heatmaps.sh bfs build
#$MQ_ROOT/scripts/smq_heatmaps.sh bfs run ctr $threads
$MQ_ROOT/scripts/smq_heatmaps.sh bfs run usa $threads
$MQ_ROOT/scripts/smq_heatmaps.sh bfs run west $threads
#$MQ_ROOT/scripts/smq_heatmaps.sh bfs run lj $threads
$MQ_ROOT/scripts/smq_heatmaps.sh bfs run twi $threads
$MQ_ROOT/scripts/smq_heatmaps.sh bfs run web $threads
$MQ_ROOT/scripts/smq_heatmaps.sh sssp build
#$MQ_ROOT/scripts/smq_heatmaps.sh sssp run ctr $threads
$MQ_ROOT/scripts/smq_heatmaps.sh sssp run usa $threads
$MQ_ROOT/scripts/smq_heatmaps.sh sssp run west $threads
#$MQ_ROOT/scripts/smq_heatmaps.sh sssp run lj $threads
$MQ_ROOT/scripts/smq_heatmaps.sh sssp run twi $threads
$MQ_ROOT/scripts/smq_heatmaps.sh sssp run web $threads
$MQ_ROOT/scripts/smq_heatmaps.sh boruvka build
#$MQ_ROOT/scripts/smq_heatmaps.sh boruvka run ctr $threads
$MQ_ROOT/scripts/smq_heatmaps.sh boruvka run usa $threads
$MQ_ROOT/scripts/smq_heatmaps.sh boruvka run west $threads
$MQ_ROOT/scripts/smq_heatmaps.sh astar build
$MQ_ROOT/scripts/smq_heatmaps.sh astar run west $threads
$MQ_ROOT/scripts/smq_heatmaps.sh astar run usa $threads
#$MQ_ROOT/scripts/smq_heatmaps.sh astar run ctr $threads
