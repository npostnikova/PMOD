source $MQ_ROOT/set_envs.sh

mkdir -p $MQ_ROOT/experiments/$CPU/plots/other_plots
cd $MQ_ROOT/experiments/$CPU/plots/other_plots

WLS=( heapswarm spraylist )
echo "Run other wls ${WLS[@]}"
for wl in "${WLS[@]}" ; do
  for t in "${PLT_THREADS[@]}"; do
    $MQ_ROOT/scripts/run_wl_all_algo.sh $wl $t $PLT_RUNS $wl
  done
done

DEF=( smq obim pmod )
echo "Run default wls ${DEF[@]}"
for wl in "${DEF[@]}" ; do
  mkdir -p $MQ_ROOT/experiments/$CPU/plots/${wl}_plots
  cd $MQ_ROOT/experiments/$CPU/plots/${wl}_plots
  for t in "${PLT_THREADS[@]}"; do
    $MQ_ROOT/scripts/run_wl_all_algo.sh "${wl}_default" $t $PLT_RUNS "${wl}_default"
  done
done


