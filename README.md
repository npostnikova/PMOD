# Multi-Queues Can Be State-of-the-Art Priority Schedulers
This repository contains everything required for running experiments 
presented in the [paper](https://arxiv.org/abs/2109.00657). 

The implementation of the designed priority schedulers can be found in 
[`Galois-2.2.1/include/Galois/WorkList/`](Galois-2.2.1/include/Galois/WorkList/):
* [`StealingMultiQueue.h`](Galois-2.2.1/include/Galois/WorkList/StealingMultiQueue.h) is the StealingMultiQueue.
* [`MQOptimized/`](Galois-2.2.1/include/Galois/WorkList/MQOptimized) contains MQ Optimized variants.
# Getting Started Guide

## Using an image
We provide images that contain all the dependencies and datasets. Images can be pulled from
[npostnikova/mq-based-schedulers](https://hub.docker.com/repository/docker/npostnikova/mq-based-schedulers) repository,
or downloaded from [Zenodo](https://zenodo.org/record/5813302).

#### 1. Select an image
As running **all** the experiments takes a very long time, we provide different image options:

* **Slow**. It computes heatmaps for the StealingMultiQueue, MQ Optimized 
(Temporal Locality for `insert()` and Task Batching for `delete()` variant), OBIM and PMOD, and performs all-thread 
execution for the default and best variants of the worklists. 

    It provides flexibility! You can change the amount of threads for the heatmaps execution,
    vary heatmap parameters to affects the scope for selecting the optimal values. 
    Changing heatmap parameters should work fine
    for the final plot computation, but keep in mind that heatmap-drawing scripts are not that 
    flexible (see details in the image `README`).
     
    *Estimated time*: 5-7 days.

* **Fast**. The image contains **pre-loaded** heatmaps, which were obtained 
 on Intel(R) Xeon(R) Gold 5218 CPU @ 2.30GHz with 64 cores. The script only computes all-thread plots for the best (according to the pre-loaded data) 
& default parameters.
    
    *Estimated time*: 30-40 hours.
    
    *Ways to speedup experiments*:  Drop executions on the small amount of threads via setting `PLT_THREADS` accordingly.
    You won't lose much as these executions are slow and 
    we were mainly focused on the large amount of threads.
#### 2. Install selected image
By now, you should have decided on which image to use:
```bash
tag=slow  # or
tag=fast
``` 
We provide two options for loading the image. 
1) Download the image from the paper artifacts:
    ```
    file=mq-based-schedulers_${tag}.tar.gz
    wget -c https://zenodo.org/record/5813302/files/$file
    sudo docker load -i $file
    ```
2) Pull the image from the Docker Hub:
    ```
    sudo docker pull npostnikova/mq-based-schedulers:$tag
    ```
#### 3. Start a container
To start a container from the image, you can use:
```
sudo docker run --cap-add SYS_NICE -it npostnikova/mq-based-schedulers:$tag
``` 
> **_NOTE:_**  Please don't neglect `--cap-add SYS_NICE` as Galois needs it
> for setting CPU affinity.

#### 4. Run experiments
1. Take a look at the `$MQ_ROOT/README.md` file inside the container.
2. Updated `$MQ_ROOT/set_envs.sh` to comply with your machine.
    ```
    nano $MQ_ROOT/set_envs.sh
    ```
3. Run experiments:
    ```
    $MQ_ROOT/run_experiments.sh
    ```
4. Navigate to Execution Results section below to learn how the 
output data is structured.

## Running experiments without Docker
#### Setup
> **_NOTE:_**  Please refer a sample [`setup.sh`](setup.sh) script.
1. Dependencies
    * A modern C++ compiler compliant with the `C++-17` standard (`gcc >= 7`, `Intel >= 19.0.1`, `clang >= 7.0`)
    * CMake
    * Boost library (the full installation is recommended)
    * Libnuma
    * Libpthread
    * Python (`>=3.7`) with matplotlib<3.5, numpy and seaborn
    * wget
    > **_Helpful links:_** 
    [Boost Installation Guide](https://www.boost.org/doc/libs/1_66_0/more/getting_started/unix-variants.html),
    [Everything you may need for Galois](https://github.com/IntelligentSoftwareSystems/Galois/blob/master/README.md).
2. Please **update** [`set_envs.sh`](set_envs.sh) script. Further, it will "configure" experiments execution.
3. Set `$MQ_ROOT` and `$GALOIS_HOME` env variables. `$MQ_ROOT` should point to 
the repository root and `GALOIS_HOME=$MQ_ROOT/Galois-2.2.1`.
4. Execute [`$MQ_ROOT/compile.sh`](compile.sh) script to build the project.
5. [`$MQ_ROOT/scripts/datasets.sh`](scripts/datasets.sh) to install and prepare all required datasets.
6. Verify that everything seems to work fine via executing 
[`$MQ_ROOT/scripts/verify_setup.sh`](scripts/verify_setup.sh).

#### Running experiments
[`$MQ_ROOT/scripts/run_all_experiments.sh`](scripts/run_all_experiments.sh) 
contains all the experiments.
But keep in mind that it is **extremely slow**. 
It includes:
* Heatmaps for SMQ, SkipList SMQ, 4 MQ Optimized versions, OBIM & PMOD.
* NUMA for SMQ & MQ Optimized.
* Baseline computation (for heatmaps and plots).
* All-thread computations for the best and default versions of all the worklists 
above + SprayList and Swarm.
* Parameters variation and all-thread execution for k-LSM.

```
# Run experiments with logging.
$MQ_ROOT/scripts/run_all_experiments.sh 2>&1 | tee $MQ_ROOT/logs.txt
```
> **_NOTE:_**  You can vary heatmap parameters as you want (i.e. change deltas for OBIM).
> It will change the scope for selecting the optimal parameters for the final plots.
> However, drawing-heatmap scripts are not that flexible and you may need to change it accordingly.

## Figures sample
Sample figures can be found in [`$MQ_ROOT/experiments/sample/pictures/`](experiments/sample/pictures).
There you can find [`heatmaps/mq_best_parameters.csv`](experiments/sample/pictures/heatmaps/mq_best_parameters.csv) 
which contains max speedup for MultiQueue-based
schedulers. Max speedups for OBIM & PMOD are reflected on their heatmaps.

## Execution results
Finally, execution results should have the following structure:
* `$MQ_ROOT/experiments/$CPU/`
    * `heatmaps/` — all heatmap executions
        * `smq_heatmaps/`
            * `numa/`
        * `slsmq_heatmaps/`
        * `mqpl_heatmaps/`
            * `numa/`
        * `obim_heatmaps/`
        * `pmod_heatmaps/`
        * `klsm_heatmaps/` — by default, doesn't present in images
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
        * `klsm_plots/` — by default, doesn't present in images
    * `baseline/` — baseline executions 
        * `*_base_$HM_THREADS`
        * `*_base_1`
    * `pictures/` — all plots
        * `heatmaps/` — corresponds to the Figures 3-14, 17-20
        * `plots/` — corresponds to the Figures 21-22   
