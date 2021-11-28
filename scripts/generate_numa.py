# Extract best wls & generate script

import sys
import csv
import os


def find_avg_for_each(file):
    name_to_sum_qnty = {}
    with open(file, newline='') as csvfile:
        data = csv.DictReader(csvfile, fieldnames=['time', 'wl', 'nodes', 'threads'])
        for row in data:
            time = int(row['time'])
            name = row['wl']
            if not name or not name[0].isalpha():
                print("Invalid row: ", row)
                continue
            cur_sum = time
            cur_qty = 1
            if name in name_to_sum_qnty:
                (prev_sum, prev_qty) = name_to_sum_qnty[name]
                cur_sum += prev_sum
                cur_qty += prev_qty
            name_to_sum_qnty[name] = (cur_sum, cur_qty)
    return {k: int(v[0] / v[1]) for k, v in name_to_sum_qnty.items()}


def find_best_name(file):
    name_to_avg = find_avg_for_each(file)
    min_name = min(name_to_avg, key=lambda x: name_to_avg[x])
    return min_name

algo_graph = []
for algo in ['bfs', 'sssp']:
    for graph in ['usa', 'west', 'twi', 'web']:
        algo_graph.append((algo, graph))

for algo in ['boruvka', 'astar']:
    for graph in ['usa', 'west']:
        algo_graph.append((algo, graph))

mq2typedef = {
    'mqpp': 'ProbProb',
    'mqpl': 'ProbLocal',
    'mqlp': 'LocalProb',
    'mqll': 'LocalLocal',
}


numa_ws = [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024]

def declaration_by_name(name):
    splitn = name.split('_')
    pref = splitn[0]
    line = ''
    if pref == 'smqhm':
        line = f'typedef StealingMultiQueue<UpdateRequest, Comparer, {splitn[1]}, {splitn[2]}> {name};'
    elif pref.startswith('slsmq'):
        line = f'typedef SkipListSMQ<UpdateRequest, Comparer, {splitn[1]}, {splitn[2]}, {w}> {wl_name};'
    elif pref in mq2typedef:
        line = f'typedef MultiQueue{mq2typedef[pref]}<UpdateRequest, Comparer, {splitn[2]}, {splitn[3]}, {splitn[1]}> {name};'
    else:
        raise f'Invalid prefix: {name}'
    line += f'\nif (wl == \"{name}\") RUN_WL({name});\n'
    return line

def numa_declaration_by_name(name, w):
    splitn = name.split('_')
    pref = splitn[0]
    splitn[0] += 'numa'
    splitn.append(str(w))
    wl_name = '_'.join(splitn)
    line = ''
    if pref == 'smqhm':
        line = f'typedef StealingMultiQueueNuma<UpdateRequest, Comparer, {splitn[1]}, {splitn[2]}, {w}> {wl_name};'
    elif pref == 'slsmqhm':
        line = f'typedef SkipListSMQNuma<UpdateRequest, Comparer, {splitn[1]}, {splitn[2]}, {w}> {wl_name};'
    elif pref in mq2typedef:
        line = f'typedef MultiQueue{mq2typedef[pref]}Numa<UpdateRequest, Comparer, {splitn[2]}, {splitn[3]}, ' \
           f'{splitn[1]}, {w}> {wl_name};'
    else:
        raise f'Invalid prefix: {name}'
    line += f'\nif (wl == \"{wl_name}\") RUN_WL({wl_name});\n'
    return line



threads = int(sys.argv[2])
wl = sys.argv[1]
heatmap_path=sys.argv[3]

algos = ['bfs', 'sssp', 'boruvka', 'astar']

def generate_best():
    algo2best_names = { algo: set() for algo in algos }
    algo2best = []
    for (algo, graph) in algo_graph:
        filename = f'{heatmap_path}/{algo}_{graph}_{wl}_{threads}'
        best_name = find_best_name(filename)
        #         best_names.add(best_name)
        algo2best_names[algo].add(best_name)
        algo2best.append((algo, graph, best_name))
    GALOIS_HOME = os.environ.get('GALOIS_HOME')
    for algo in algos:
        exp_filename = f'{GALOIS_HOME}/apps/{algo}/Experiments.h'
        exp_file = open(exp_filename, 'a')  # TODO
        for name in algo2best_names[algo]:
            line = declaration_by_name(name)
            exp_file.write(line)
        exp_file.close()
    MQ_ROOT = os.environ.get('MQ_ROOT')
    script = open(f'run_best.sh', 'w')  # TODO w or a
    for (algo, graph, name) in algo2best:
        res_suff = f'{wl}_best' # TODO with wl or without
        script.write(f'$MQ_ROOT/scripts/run_wl_n_times.sh {algo} {graph} {name} $threads $times {res_suff}\n')
    script.close()


def generate_best_numa():
    algo2best_names = { algo: set() for algo in algos }
    algo2best = []
    for (algo, graph) in algo_graph:
        filename = f'{heatmap_path}/{algo}_{graph}_{wl}_{threads}'
        best_name = find_best_name(filename)
        #         best_names.add(best_name)
        algo2best_names[algo].add(best_name)
        algo2best.append((algo, graph, best_name))
    GALOIS_HOME = os.environ.get('GALOIS_HOME')
    for algo in algos:
        exp_filename = f'{GALOIS_HOME}/apps/{algo}/Experiments.h'
        exp_file = open(exp_filename, 'w')
        for name in algo2best_names[algo]:
            for w in numa_ws:
                line = numa_declaration_by_name(name, w)
                exp_file.write(line)
        exp_file.close()
    MQ_ROOT = os.environ.get('MQ_ROOT')
    script = open(f'run_numa.sh', 'w')
    script.write('set -e\n')
    script.write('source $MQ_ROOT/set_envs.sh\n')
    for (algo, graph, name) in algo2best:
        for w in numa_ws:
            splitn = name.split('_')
            splitn[0] += 'numa'
            splitn.append(str(w))
            numa_name = '_'.join(splitn)
            res_suff = f'{wl}_numa' # TODO with wl or without
            script.write(f'$MQ_ROOT/scripts/run_wl_n_times.sh {algo} {graph} {numa_name} $HM_THREADS $HM_RUNS {res_suff}\n')
    script.close()

generate_best_numa()
