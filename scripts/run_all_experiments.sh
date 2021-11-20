set -e

source $MQ_ROOT/set_envs.sh

################## HEATMAPS ##################
echo "Computing heatmaps. Be patient!"

hm_dir=$MQ_ROOT/experiments/$CPU/heatmaps
echo "Directory for heatmaps: $hm_dir"

run_hm() {
  subdir=$1
  script=$2
  results_location=$hm_dir/$subdir
  mkdir -p $results_location
  cd $results_location
  echo "Saving heatmaps onto: $results_location"
  $MQ_ROOT/scripts/$script $HM_THREADS $3
}

run_mq_hm() {
  mq=$1
  run_hm "${mq}_heatmaps" run_mq_optimized_hm.sh $mq
}

echo "Running StealingMultiQueue via d-ry heaps heatmaps"
run_hm smq_heatmaps run_smq_hm.sh

echo "Running StealingMultiQueue via lock-free skiplists heatmaps"
run_hm slsmq_heatmaps run_slsmq_hm.sh

echo "Running MQ Optimized [Temporal Locality for insert() & delete()] heatmaps"
run_mq_hm mqpp

echo "Running MQ Optimized [Temporal Locality for insert() & Task Batching for delete()] heatmaps"
run_mq_hm mqpl

echo "Running MQ Optimized [Task Batching for insert() & Temporal Locality delete()] heatmaps"
run_mq_hm mqlp

echo "Running MQ Optimized [Task Batching for insert() & delete()] heatmaps"
run_mq_hm mqll

echo "Running OBIM & PMOD heatmaps"
run_hm obim_heatmaps run_obim_params.sh


################## PLOTS ##################
# Running best worklists on different amount of threads (specified in PLT_THREADS).

echo "Running best SMQ combinations on all threads"
$MQ_ROOT/scripts/run_best_smq.sh smq

echo "Running best SkipList SMQ combinations on all threads"
$MQ_ROOT/scripts/run_best_smq.sh slsmq

echo "Running best MQ variants on all threads"
$MQ_ROOT/scripts/run_best_mq.sh

echo "Running other worklists"
mkdir -p $MQ_ROOT/experiments/$CPU/plots/other_plots
cd $MQ_ROOT/experiments/$CPU/plots/other_plots
$MQ_ROOT/scripts/run_other_worklists_all.sh