
wl=smq
pyscript=~/PMOD/experiments/mq_optimized_hm/find_best.py
t=256
for algo in bfs sssp; do
  for graph in usa west twi web; do
    echo ">>>>"
    echo "$algo $graph"
    python3.7 $pyscript "../${algo}_${graph}_${wl}_$t"
    python3.7 $pyscript "${algo}_${graph}_${wl}_numa"
    python3.7 $pyscript \
      "$MQ_ROOT/experiments/find_params_obim/${algo}_${graph}_obim_$t"
    python3.7 $pyscript \
      "$MQ_ROOT/experiments/find_params_obim/${algo}_${graph}_pmod_$t"
  done
done

for algo in boruvka astar; do
  for graph in usa west; do
    echo ">>>>"
    echo "$algo $graph"
    python3.7 $pyscript "../${algo}_${graph}_${wl}_$t"
    python3.7 $pyscript "${algo}_${graph}_${wl}_numa"
    python3.7 $pyscript \
      "$MQ_ROOT/experiments/find_params_obim/${algo}_${graph}_obim_$t"
    python3.7 $pyscript \
      "$MQ_ROOT/experiments/find_params_obim/${algo}_${graph}_pmod_$t"
  done
done
