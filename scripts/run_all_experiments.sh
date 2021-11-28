#set -e

source $MQ_ROOT/set_envs.sh

################## HEATMAPS ##################
echo "Computing heatmaps. Be patient!"

hm_dir=$MQ_ROOT/experiments/$CPU/heatmaps
plt_dir=$MQ_ROOT/experiments/$CPU/plots
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

echo "Running OBIM heatmaps"
run_hm obim_heatmaps run_obim_hm.sh

echo "Running PMOD heatmaps"
run_hm pmod_heatmaps run_pmod_hm.sh

echo "Running kLSM params"
run_hm klsm_heatmaps run_klsm_hm.sh


################## BASELINE ##################
base_dir=$MQ_ROOT/experiments/$CPU/baseline
mkdir -p $base_dir
cd $base_dir
echo "Computing baseline in $base_dir"
$MQ_ROOT/scripts/run_wl_all_algo.sh "hmq$MQ_C" $HM_THREADS $HM_RUNS "base_$HM_THREADS"
$MQ_ROOT/scripts/run_wl_all_algo.sh "hmq$MQ_C" 1 $HM_RUNS "base_1"


################## NUMA ##################

if [[ "$NUMA_NODES" == "2" || "$NUMA_NODES" == "4" ]]; then
  echo "Vary NUMA weights for best heatmap combinations"
  $MQ_ROOT/scripts/run_best_numa.sh smq
  $MQ_ROOT/scripts/run_best_numa.sh mqpp
  $MQ_ROOT/scripts/run_best_numa.sh mqpl
  $MQ_ROOT/scripts/run_best_numa.sh mqlp
  $MQ_ROOT/scripts/run_best_numa.sh mqll
else
  echo "NUMA execution for $NUMA_NODES nodes not supported"
fi

################## PLOTS ##################
# Running best worklists on different amount of threads (specified in PLT_THREADS).

echo "Running best SMQ combinations on all threads"
$MQ_ROOT/scripts/run_best_wl.sh smq

echo "Running best SkipList SMQ combinations on all threads"
$MQ_ROOT/scripts/run_best_wl.sh slsmq

echo "Running best OBIM combinations on all threads"
$MQ_ROOT/scripts/run_best_wl.sh obim

echo "Running best PMOD combinations on all threads"
$MQ_ROOT/scripts/run_best_wl.sh pmod

echo "Running best MQ variants on all threads"
echo "Warning! Runs MQPL only, please change run_best_mq script if itsn't what you need"
$MQ_ROOT/scripts/run_best_mq.sh

echo "Running defult SMQ, OBIM, PMOD and other worklists"
$MQ_ROOT/scripts/run_other_worklists.sh

echo "Running best kLSM combinations on all threads"
$MQ_ROOT/scripts/run_best_wl.sh klsm

################## DRAWING PICTURES ##################
pic_dir=$MQ_ROOT/experiments/$CPU/pictures

echo "Drawing heatmaps"

mkdir -p $pic_dir/heatmaps
cd $pic_dir/heatmaps
$PYTHON_EXPERIMENTS $MQ_ROOT/scripts/draw_all_heatmaps.py

echo "Drawing plots"
mkdir -p $pic_dir/plots
cd $pic_dir/plots
$PYTHON_EXPERIMENTS $MQ_ROOT/scripts/draw_plots_all.py "${PLT_THREADS[@]}"

echo "That's all! I hope, nothing has failed..."
