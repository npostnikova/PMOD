from plots.heatmap_lib import find_avg_hmq

import csv
import sys
import os

weights = [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024]

def find_avg_for_each(file, hmq_time):
    weight2cnt = { w: 0 for w in weights }
    weight2sum = { w: 0 for w in weights }
    with open(file, newline='') as csvfile:
        data = csv.DictReader(csvfile, fieldnames=['time', 'wl', 'nodes', 'threads'])
        for row in data:
            time = int(row['time'])
            name = row['wl']
            if not name or not name[0].isalpha():
                print("Invalid row: ", row)
                continue
            w = int(name.split('_')[-1])
            if w not in weight2sum:
                print("Invalid weight: ", w)
                continue
            weight2sum[w] += time
            weight2cnt[w] += 1
    return [ round(hmq_time / (weight2sum[w] / weight2cnt[w]), 2) for w in weights]


path_prefix = f"{os.environ.get('MQ_ROOT')}/experiments/{os.environ.get('CPU')}/"
hm_threads = os.environ.get('HM_THREADS')

algo_gr = []
for algo in ['bfs', 'sssp']:
    for gr in ['usa', 'west', 'twi', 'web']:
        algo_gr.append((algo, gr))

for algo in ['astar', 'boruvka']:
    for gr in ['usa', 'west']:
        algo_gr.append((algo, gr))

headers = []
for algo in ['bfs', 'sssp']:
    for gr in ['usa', 'west', 'twitter', 'web']:
        headers.append((algo.upper(), gr.upper()))
for algo in ['a*', 'mst']:
    for gr in ['usa', 'west']:
        headers.append((algo.upper(), gr.upper()))

wl = sys.argv[1]

f = open(f'{wl}_numa.tex', 'w')

for i, (algo, gr) in enumerate(algo_gr):
    hmq_time = find_avg_hmq(f'{path_prefix}/baseline/{algo}_{gr}_base_{hm_threads}')[0]
    path = f'{path_prefix}/heatmaps/{wl}_heatmaps/numa/{algo}_{gr}_{wl}_numa'
    vals = find_avg_for_each(path, hmq_time)
    max_val = max(vals)
    f.write(f'\large{{\\textbf{{{headers[i][0]} {headers[i][1]}}}}} &  ')
    speedups = ' & '.join([str(x) if x != max_val else f'\color{{Numa}}{{\\textbf{{{x}}}}}' for x in vals])
    f.write(speedups)
    f.write(' \\\\ \n \\hline \n')






