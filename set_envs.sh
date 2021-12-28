# Threads used for drawing plots.
# We use power of two until logical CPU number.
# !! Mustn't exceed the number of CPUs. !!
PLT_THREADS=(1 2 4 8 16 32 64 128)

# How many times worklist for each thread should be executed.
PLT_RUNS=5
