# MQ-Based Schedulers: Not Too Slow Experiments
Seems like you want to achieve more fair results for OBIM & PMOD, good choice!

MQ & SMQ heatmaps are already computed and stored in `$MQ_ROOT/executions/sample/heatmaps`.
You will only run heatmaps for OBIM/PMOD and then the best and default worklist versions.

For this configuration, most of the scripts were simplified.
However, there are still a couple of steps left for you.

```bash
nano $MQ_ROOT/set_envs.sh  # Please update if needed!
$MQ_ROOT/run_experiments.sh
```
See details below.

## Configure `set_envs.sh`
It is the most important step. 
1. Please specify `PLT_THREADS` 
which sets on which threads worklists should be executed. Normally, we take 
powers of 2 til the number of logical CPUs.

2. Specify `OBIM_HM_THREADS`. It's the amount of threads for OBIM/PMOD heatmaps.
Should be the number of logical CPUs.

3. Adjust `PLT_RUNS` & `HM_RUNS` if needed. For plots you can see in the paper, we used `PLT_RUNS=10`. 
However, to make the script faster you can use less. For all heatmaps in the paper we used `HM_RUNS=5`, but
you can change it as well.

4. You can configure OBIM heatmap parameters. If you want to use other deltas or chunk sizes, go ahead*!

> **_NOTE*:_**  Changing parameters will work fine for final plots. However,
> drawing heatmap scripts do not support this yet (so you miss a nice heatmap picture). Anyway, I can help with that :)

5. Please don't change other fields. They correspond to the pre-computed data.

## Run `run_experiments.sh`  
This executes all required experiments to draw plots.
Execution info (such as time & amount of work for worklist) is located in `$MQ_ROOT/experiments/sample/plots/`.
Heatmap executions for OBIM/PMOD are located in `$MQ_ROOT/experiments/sample/heatmaps/` in `obim_heatmaps` and `pmod_heatmaps` folders.
Plots themselves can be found in `$MQ_ROOT/experiments/sample/pictures/`.