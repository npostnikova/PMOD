import os
import numpy as np
from plots.heatmap_lib import *

def find_best_divergence(filename, time_div, nodes_div):
    hm, _ = heatmap2array(filename, time_div, nodes_div)
    print(filename, hm[3][1])
    max_speedup = np.amax(hm)
    for i in range(0, 11):
        for j in range(0, 11):
            hm[i][j] = max_speedup - hm[i][j]
    return hm

def find_best_params(files, baseline_files):
    fee_sum = np.zeros((11, 11))
    for i in range(0, len(files)):
        base_time, base_nodes = find_avg_hmq(baseline_files[i])
        hm = find_best_divergence(files[i], base_time, base_nodes)
        fee_sum += hm
    print("========")
    # print(fee_sum)
    # print(np.amin(fee_sum))
    # print(fee_sum[3][3])
    return np.argmin(fee_sum)


threads = os.environ['HM_THREADS']
mq_root = os.environ['MQ_ROOT']
cpu = os.environ['CPU']
prefix = f'{mq_root}/experiments/{cpu}/'

wl = "smq"

files= [
    f'{prefix}/heatmaps/{wl}_heatmaps/sssp_usa_{wl}_{threads}',
    f'{prefix}/heatmaps/{wl}_heatmaps/sssp_west_{wl}_{threads}',
    f'{prefix}/heatmaps/{wl}_heatmaps/sssp_twi_{wl}_{threads}',
    f'{prefix}/heatmaps/{wl}_heatmaps/sssp_web_{wl}_{threads}',
    f'{prefix}/heatmaps/{wl}_heatmaps/bfs_usa_{wl}_{threads}',
    f'{prefix}/heatmaps/{wl}_heatmaps/bfs_west_{wl}_{threads}',
    f'{prefix}/heatmaps/{wl}_heatmaps/bfs_twi_{wl}_{threads}',
    f'{prefix}/heatmaps/{wl}_heatmaps/bfs_web_{wl}_{threads}',
    f'{prefix}/heatmaps/{wl}_heatmaps/astar_usa_{wl}_{threads}',
    f'{prefix}/heatmaps/{wl}_heatmaps/astar_west_{wl}_{threads}',
    f'{prefix}/heatmaps/{wl}_heatmaps/boruvka_usa_{wl}_{threads}',
    f'{prefix}/heatmaps/{wl}_heatmaps/boruvka_west_{wl}_{threads}',
]
baseline = [
    f'{prefix}/baseline/sssp_usa_base_{threads}',
    f'{prefix}/baseline/sssp_west_base_{threads}',
    f'{prefix}/baseline/sssp_twi_base_{threads}',
    f'{prefix}/baseline/sssp_web_base_{threads}',
    f'{prefix}/baseline/bfs_usa_base_{threads}',
    f'{prefix}/baseline/bfs_west_base_{threads}',
    f'{prefix}/baseline/bfs_twi_base_{threads}',
    f'{prefix}/baseline/bfs_web_base_{threads}',
    f'{prefix}/baseline/astar_usa_base_{threads}',
    f'{prefix}/baseline/astar_west_base_{threads}',
    f'{prefix}/baseline/boruvka_usa_base_{threads}',
    f'{prefix}/baseline/boruvka_west_base_{threads}',
]

print(find_best_params(files, baseline))
