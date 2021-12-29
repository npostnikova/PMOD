# MQ-Based Schedulers: Fast Experiments
To save your time, computed heatmaps are stored in `$MQ_ROOT/executions/sample/heatmaps`.
You will only run the best and default worklist versions. The estimated run time is
30-40 hours.

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
powers of 2 til the number of logical CPUs. It **mustn't** exceed the number of logical CPUs.

2. Adjust `PLT_RUNS` if needed. For plots you can see in the paper, we used 10. 
However, to make the script "super fast" you can use less.

> **_NOTE:_** To run `k-LSM`, uncomment lines `27-28` in `$MQ_ROOT/scripts/run_all_experiments.sh` and the line `24` in `$MQ_ROOT/scripts/plots/draw_plots_helper.py`. But it's incredibly slow for some benchmarks on small amount of threads.


## Run `run_experiments.sh`  
This executes all required experiments to draw plots.
Execution info (such as time & amount of work for worklist) is located in `$MQ_ROOT/experiments/sample/plots/`.

Plots themselves can be found in `$MQ_ROOT/experiments/sample/pictures/plots/`.

Logs are recorded to `$MQ_ROOT/logs.txt` file.

To draw heatmaps from the pre-loaded data, you can use:
```
mkdir -p $MQ_ROOT/experiments/$CPU/pictures/heatmaps
cd $MQ_ROOT/experiments/$CPU/pictures/heatmaps
$PYTHON_EXPERIMENTS $MQ_ROOT/scripts/draw_all_heatmaps.py
```
Heatmaps will be located in `$MQ_ROOT/experiments/sample/pictures/heatmaps/`