# MQ-Based Schedulers: Slow Experiments
Brave adventurer, good luck! I warned you: it's going to be really slow.

You are going to compute heatmaps for SMQ, SkipList SMQ, MQ Optimized (TL for `insert()`
and B for `delete()`), OBIM & PMOD.
After that, you will run all these worklist on all specified threads.

```bash
nano $MQ_ROOT/set_envs.sh  # Please update if needed!
$MQ_ROOT/run_experiments.sh
```
See details below.

## Configure `set_envs.sh`
It is the most important step. 
1. Please specify `PLT_THREADS` 
which sets on which threads worklists should be executed for the final plot. Normally, we take 
powers of 2 til the number of logical CPUs. **Mustn't** exceed the number of logical CPUs!!

2. Specify `HM_THREADS`. It's the amount of threads for heatmaps.
Should be the number of logical CPUs.

3. Specify `NUMA_NODES` to reflect the number of sockets you have. Currently, 
NUMA variant will be executed only for `2` and `4` nodes. 

4. Adjust `PLT_RUNS` & `HM_RUNS` if needed. For plots you can see in the paper, we used `PLT_RUNS=10`. 
However, to make the script faster you can use less. For all heatmaps in the paper we used `HM_RUNS=5`, but
you can change it as well.

5. You can vary heatmap parameters (such as `delta`, `chunk_size` for OBIM and 
`steal_size` & `steal_prob` for SMQ). It will affect the scope from which the best params are selected.

    > **_NOTE:_**  Changing parameters will work fine for final plots. However,
    drawing heatmap scripts do not support this yet (so you miss a nice heatmap picture). Anyway, I can help with that :)
    Btw, if you just remove some vals (i.e. using `deltas = (0 2 8)`), you should be able to use existing drawing scripts, having
    zeros for removed values.

6. You can add k-LSM execution. I lost my temper while waiting for one thread
execution but you can give it a try. In `$MQ_ROOT/scripts/run_all_experiments.sh`
uncomment lines `51-52`, `99-100` and the line `26` in `$MQ_ROOT/scripts/plots/draw_plots_helper.py`.

7. Also, you may want to try different MQ Optimized variants. In the script, we currently draw heatmaps and find 
the best parameters for `insert=Temporal Locality, delete=Task Batching` version.
To update that, please take a look at `$MQ_ROOT/scripts/run_all_experiments.sh`, 
lines `33-43`, `64-75`, `$MQ_ROOT/scripts/run_best_mq.sh` line `8`, and `$MQ_ROOT/scripts/plots/draw_plots_helper.py` line `18`.
In the plot drawing script, path suffix should be `_numa` for MQ Optimized with NUMA.

MQ Optimized variants:
* mqpp -- Temporal Locality (TL) for `insert` and `delete`
* mqll -- Task Batching (TB) for `insert` and `delete`
* mqpl -- TL for `insert`, TB for `delete`
* mqlp -- TB for `insert`, TL for `delete`

## Run `run_experiments.sh`  
This executes all required experiments to draw plots.
Execution info (such as time & amount of work for worklist) is located in `$MQ_ROOT/experiments/$CPU/plots/`.
Heatmap executions are located in `$MQ_ROOT/experiments/$CPU/heatmaps/`.

Plots themselves can be found in `$MQ_ROOT/experiments/sample/pictures/`. To take a look at sample figures
[follow the link](https://github.com/npostnikova/mq-based-schedulers/tree/master/experiments/sample/pictures).
