# MQ-Based Schedulers: Super Fast Experiments
To save your time, computed heatmaps are stored in `$MQ_ROOT/executions/sample/heatmaps`.
You will only run the best and default worklist versions.

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

2. Adjust `PLT_RUNS` if needed. For plots you can see in the paper, we used 10. 
However, to make the script "super fast" you can use less.

3. Please don't change other fields. They help to make execution more flexible 
for those who want to spend ages computing heatmaps.

## Run `run_experiments.sh`  
This executes all required experiments to draw plots.
Execution info (such as time & amount of work for worklist) is located in `$MQ_ROOT/experiments/sample/plots/`.

Plots themselves can be found in `$MQ_ROOT/experiments/sample/pictures/plots/`.