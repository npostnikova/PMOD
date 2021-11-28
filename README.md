# MQ-Based Schedulers: Extra Slow Experiments
Brave adventurer, good luck! I warned you: it's going to be really slow.

You are going to compute heatmaps for SMQ, SkipList SMQ, MQ Optimized (TL for `insert()`
and B for `delete()`), OBIM & PMOD. After that, you will run all these worklist on 
all specified threads.

```bash
nano $MQ_ROOT/set_envs.sh  # Please update if needed!
$MQ_ROOT/run_experiments.sh
```
See details below.

## Configure `set_envs.sh`
It is the most important step. 
1. Please specify `PLT_THREADS` 
which sets on which threads worklists should be executed for the final plot. Normally, we take 
powers of 2 til the number of logical CPUs.

2. Specify `HM_THREADS`. It's the amount of threads for heatmaps.
Should be the number of logical CPUs.

3. Specify `NUMA_NODES` to reflect the number of sockets you have. 

4. Adjust `PLT_RUNS` & `HM_RUNS` if needed. For plots you can see in the paper, we used `PLT_RUNS=10`. 
However, to make the script faster you can use less. For all heatmaps in the paper we used `HM_RUNS=5`, but
you can change it as well.

5. You can vary heatmap parameters (such as `delta`, `chunk_size` for OBIM and 
`steal_size` & `steal_prob` for SMQ). It will affect the scope from which the best params are selected.

> **_NOTE*:_**  Changing parameters will work fine for final plots. However,
> drawing heatmap scripts do not support this yet (so you miss a nice heatmap picture). Anyway, I can help with that :)
> Btw, if you just remove some vals (i.e. using `deltas = (0 2 8)`), you should be able to use existing drawing scripts, having
> zeros for removed values.


## Run `run_experiments.sh`  
This executes all required experiments to draw plots.
Execution info (such as time & amount of work for worklist) is located in `$MQ_ROOT/experiments/sample/plots/`.
Heatmap executions for OBIM/PMOD are located in `$MQ_ROOT/experiments/sample/heatmaps/` in `obim_heatmaps` and `pmod_heatmaps` folders.
Plots themselves can be found in `$MQ_ROOT/experiments/sample/pictures/`.