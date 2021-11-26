set -e

source $MQ_ROOT/set_envs.sh

threads=128

$MQ_ROOT/scripts/pmod_heatmaps.sh sssp build
#$MQ_ROOT/scripts/obim_heatmaps.sh sssp run lj $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/pmod_heatmaps.sh sssp run twi $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
$MQ_ROOT/scripts/pmod_heatmaps.sh sssp run web $threads 0 1024 $DELTA_SMALL_MIN $DELTA_SMALL_MAX
