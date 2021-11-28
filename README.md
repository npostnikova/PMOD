# Multi-Queues Can Be State-of-the-Art Priority Schedulers

# Getting Started Guide

To save time, it is recommended to use images from 
[npostnikova/mq-based-schedulers](https://hub.docker.com/repository/docker/npostnikova/mq-based-schedulers) repository.

## Using an image
#### 1. Select an image
All settings (such as system-dependent parameters) are needed to be configured in `set_envs.sh`. 
Each image has its own requirements which are reflected in `set_envs.sh` and `README.md` of **that** image.

As running **all** the experiments is sort of a nightmare, we suggest different image options:

* **Super Fast**. No, it is not as fast as you could think. The image has **pre-loaded** heatmaps for worklists, so that
the script only computes all-thread plots for the best & default parameters.
    
    *Estimated time*: Can take the whole day.
    
    *Ways to speedup*:  Drop small threads via setting `PLT_THREADS` accordingly.
    Anyway, small amount of threads are slow and they were not the main focus, but the plots may become uglier.
    
* **Not Too Slow**. Well, it's not the slowest option. It allows to evaluate OBIM and PMOD in a more fair way.
Heatmaps for SMQ and MQ Optimized are pre-loaded, but for OBIM and PMOD they aren't! So, besides all-thread executions, 
these heatmaps are computed as well.
   
   *Estimated time*: May take a couple of days. We narrowed parameter values as we knew what the optimal OBIM parameters
   should look like beforehand. However, don't expect it to be vary fast as small deviation from the optimal value 
   can impact performance dramatically.
   
* **Extra Slow**. Yeah, this name is correct! It computes **all** heatmaps and 
all-thread plots. Well, not all, for MQ Optimized only Temporal Locality for `insert()` and Task Batching for `delete()` 
is considered, which decreases computation time at least by 2. 

    It provides more flexibility! You can change the amount of threads for heatmaps,
    vary heatmap parameters (which affects the scope for selecting the optimal values). It should work fine
    for final plot computation, but keep in mind that heatmap-drawing scripts are not that adaptive (see details in the image `README`).
     
    *Estimated time*: **Not Too Slow** + a few of days.

#### 2. Use selected image
Let's assume that you chose image `tag` (which is one of `super-fast-experiments`, 
`not-too-slow-experiments` or `extra-slow-experiments`).

```
docker pull npostnikova/mq-based-schedulers:$tag
docker run --cap-add SYS_NICE -it npostnikova/mq-based-schedulers:$tag
``` 
> **_NOTE:_**  Please don't neglect `--cap-add SYS_NICE` as Galois needs it
> for setting CPU affinity.

Inside the container, you need update `set_envs.sh` to update experiments settings (no panic, just a couple of fields like
which threads to run) and run the experiments.
```
nano $MQ_ROOT/set_envs.sh    # Update settings
$MQ_ROOT/run_experiments.sh
```
See `README.md` in your image for details.

## For those who DO NOT use images
Good luck! You may want to use another branch which contains less experiments (see the image options). But please take a look at **this** `README` 
anyway as branches for images specify how to set up `set_envs.sh` only.
#### Setup
> **_NOTE:_**  Please refer a sample `setup.sh` script.
> The script should be called from the repository root. Don't forget to update `set_envs.sh`!  
1. Dependencies
    * A modern C++ compiler compliant with the `C++-17` standard (`gcc >= 7`, `Intel >= 19.0.1`, `clang >= 7.0`)
    * CMake
    * Boost library (the full installation is recommended)
    * Libnuma
    * Libpthread
    * Python (`>= 3.7`) with matplotlib, numpy and seaborn
    * wget
    > **_Helpful links:_** 
    [Boost Installation Guide](https://www.boost.org/doc/libs/1_66_0/more/getting_started/unix-variants.html),
    [Everything you may need for Galois](https://github.com/IntelligentSoftwareSystems/Galois/blob/master/README.md).
2. Please **update** `set_envs.sh` script. Further, it will "configure" experiments execution.
3. Set `$MQ_ROOT` and `$GALOIS_HOME` env variables. It can be done by execution `source set_envs.sh` in the repository root.
    `$MQ_ROOT` should point to the repository root and `$GALOIS_HOME=$MQ_ROOT/Galois-2.2.1`.
4. Execute `$MQ_ROOT/compile.sh` script to build the project.
5. `$MQ_ROOT/scripts/datasets.sh` to install and prepare all required datasets.
6. Let's check if everything seems to work fine! Execute `$MQ_ROOT/scripts/verify_setup.sh`.

#### Running experiments
`$MQ_ROOT/scripts/run_all_experiments.sh` contains all the experiments you need.
But keep in mind that it is **extremely slow**. 
It contains:
* Heatmaps for SMQ, SkipList SMQ, 4 MQ Optimized versions, OBIM & PMOD.
* NUMA for SMQ & MQ Optimized.
* Baseline computation (for heatmaps and plots).
* All-thread computation for all worklists above + SprayList and Swarm.

> **_NOTE:_**  You can vary heatmap parameters as you want (i.e. change deltas for OBIM).
> It will change the scope for selecting the optimal parameters for final plots.
> However, drawing-heatmap scrips are not that flexible and you may need to change it accordingly.

## When something doesn't work
It will likely be my fault, I'm sorry in advance. Please don't hesitate to contact me with any questions.

## Worklist implementation
If you want to take a look at StealingMultiQueue and other MQ-based schedulers,
please follow `$GALOIS_HOME/include/Galois/WorkList/`.
* `StealingMultiQueue.h` is the StealingMultiQueue.
* `MQOptimized/` contains MQ Optimized variants.

## Heatmap sample
Sample heatmaps can be found in `$MQ_ROOT/experiments/sample/pictures/heatmaps`.
There you can find `mq_best_parameters.csv` which contains max speedup for MultiQueue-based
schedulers. Max speedup for OBIM & PMOD is reflected on their heatmaps.

## Experiment results structure
Finally, results should have the following structure:
* `$MQ_ROOT/experiments/$CPU/`
    * `heatmaps/` — all heatmap executions
        * `smq_heatmaps/`
            * `numa/`
        * `slsmq_heatmaps/`
        * `mqpl_heatmaps/`
            * `numa/`
        * `obim_heatmaps/`
        * `pmod_heatmaps/`
    * `plots/` — all plot executions
        * `smq_plots/`
            * `*_smq`
            * `*_smq_numa`
            * `*_smq_default`
        * `slsmq_plots/`
        * `mqpl_plots/`
            * `*_mqpl`
            * `*_mqpl_numa`
        * `obim_plots/`
            * `*_obim`
            * `*_obim_default`
        * `pmod_plots/`
            * `*_pmod`
            * `*_pmod_default`
         * `other_plots/`
            * `*_spraylist`
            * `*_heapswarm`
    * `baseline/` — baseline executions 
        * `*_base_$HM_THREADS`
        * `*_base_1`
    * `pictures/` — all plots
        * `heatmaps/`
        * `plots/`
    