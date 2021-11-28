set -e

threads=$1
mq=$2

$MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq bfs build
$MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq bfs run usa $threads
$MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq bfs run west $threads
$MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq bfs run twi $threads
$MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq bfs run web $threads
$MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq sssp build
$MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq sssp run usa $threads
$MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq sssp run west $threads
$MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq sssp run twi $threads
$MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq sssp run web $threads
$MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq boruvka build
$MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq boruvka run usa $threads
$MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq boruvka run west $threads
$MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq astar build
$MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq astar run west $threads
$MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq astar run usa $threads
#$MQ_ROOT/scripts/mq_optimized_heatmaps.sh $mq astar run ctr $threads
