scr=$MQ_ROOT/scripts/run_wl_all_algo.sh
for t in 1 2 4 8 16 32 64 128; do
  bash $scr obim $t 5 obimdef
done

for t in 1 2 4 8 16 32 64 128; do
  bash $scr adap-obim $t 5 obimdef
done
