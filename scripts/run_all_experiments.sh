#set -e

source $MQ_ROOT/set_envs.sh

################## PLOTS ##################
# Running best worklists on different amount of threads (specified in PLT_THREADS).

echo "Running best SMQ combinations on all threads"
$MQ_ROOT/scripts/run_best_smq.sh smq

echo "Running best SkipList SMQ combinations on all threads"
$MQ_ROOT/scripts/run_best_smq.sh slsmq

echo "Running best OBIM combinations on all threads"
$MQ_ROOT/scripts/run_best_smq.sh obim

echo "Running best PMOD combinations on all threads"
$MQ_ROOT/scripts/run_best_smq.sh pmod

echo "Running best MQ variants on all threads"
echo "Warning! Runs MQPL only, please change run_best_mq script if itsn't what you need"
$MQ_ROOT/scripts/run_best_mq.sh

echo "Running defult SMQ, OBIM, PMOD and other worklists"
$MQ_ROOT/scripts/run_other_worklists.sh


################## DRAWING PICTURES ##################
pic_dir=$MQ_ROOT/experiments/$CPU/pictures

echo "Drawing plots"
mkdir -p $pic_dir/plots
cd $pic_dir/plots
$PYTHON_EXPERIMENTS $MQ_ROOT/scripts/draw_plots_all.py "${PLT_THREADS[@]}"

echo "That's all! I hope, nothing has failed..."
