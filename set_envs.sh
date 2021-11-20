######### ACTION REQUIRED #########

# Absolute path to the clonned repository.
export MQ_ROOT=~/PMOD

# To store experiments for various CPUs in different locations.
export CPU=intel

# Number of logical CPUs (including hyperthreading).
# If you don't want heatmaps to use these many threads, please
# update HM_THREADS below.
export MAX_CPU_NUM=128

# Number of NUMA nodes.
# Used *only* for SMQ & MQ Opitimized NUMA experiments.
# Currently, only 2 and 4 nodes are supported for the experiments.
export NUMA_NODES=2

# The version of python to use for scripts.
export PYTHON_EXPERIMENTS=python3.8

# Threads used for drawing plots.
# We use power of two until logical CPU number.
PLT_THREADS=(1 2 4 8 16 32 64 128)

################################################

# Path to the Galois library.
export GALOIS_HOME=$MQ_ROOT/Galois-2.2.1/

######### HEATMAPS #########

# Number of threads to count heatmaps.
# We use the number logical CPUs (hyperthreading included).
export HM_THREADS=$MAX_CPU_NUM

# How many times one set of parameters is run.
export HM_RUNS=5

# Values for the first and second heatmap parameters (y and x axis).
# For instance, for SMQ stealing prob = 1 / HM_FST[i] and stealing size = HM_SND[j].
HM_FST=( 1 2 4 8 16 32 64 128 256 512 1024 )
HM_SND=( 1 2 4 8 16 32 64 128 256 512 1024 )

#### OBIM/PMOD heatmaps

# Delta shift values.
DELTAS=( 0 2 4 8 10 12 14 16 18 )
# Chunk sizes.
CHUNK_SIZE=( 32 64 128 256 512 )

# In "Understanding Priority-Based Scheduling of Graph Algorithms on a
# Shared Memory Platform" work, Yesil et al found optimal deltas for OBIM,
# depending on benchmark and graph type.
# For BFS and MST delta=0 was selected. For SSSP and A*, delta=0 for
# web graphs and delta=14 for real-world road graphs.
# As long as non optimal delta can lead to unbearably slow execution, we
# decided not to go through all deltas, but to investigate the neighbourhood.

# Used for BFS, MST and SSSP & A* with network graphs.
# The following deltas to be reflected on heatmaps:
# { d | d in DELTAS & d >= DELTA_SMALL_MIN & d <= DELTA_SMALL_MAX }
DELTA_SMALL_MIN=0
DELTA_SMALL_MAX=10

# USED for SSSP and A* with road graphs.
DELTA_LARGE_MIN=10
DELTA_LARGE_MAX=18


######### PLOTS #########

# How many times worklist for each thread should be executed.
PLT_RUNS=10