import matplotlib.ticker as tckr
import matplotlib.ticker as mticker
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

import glob
import sys
import statistics
import numpy as np

class ExecSum:
    def __init__(self, thr):
        self.threads = thr
        self.time = []
        self.nodes = []
        self.qty = 0

    def add_exec(self, t, n):
        self.time.append(t)
        self.nodes.append(n)
        self.qty += 1

    def avg_time(self):
        return sum(time) / qty

    def avg_nodes(self):
        return sum(time) / qty

    def __str__(self):
        return f"{self.threads} {round(sum(self.time) / self.qty, 3)} {round(sum(self.nodes) / self.qty, 3)}"

    def compute(self, time_div, nodes_div):
        if self.qty == 1:
            print(f'Qty1 for threads {self.threads}')
        return { 'threads': self.threads,
                 'time': time_div / (sum(self.time) / self.qty),
                 'nodes': (sum(self.nodes) / self.qty) / nodes_div,
                 'timestd': statistics.stdev([time_div / x for x in self.time]),
                 'nodesstd': statistics.stdev([x / nodes_div for x in self.nodes])
                 }


class WlExec:
    def __init__(self, name):
        self.name = name
        self.execs = {}

    def add_exec(self, thr, t, n):
        if thr not in self.execs:
            self.execs[thr] = ExecSum(thr)
        self.execs[thr].add_exec(t, n)

    def avg_time(self, thr):
        es = self.execs[thr]
        return sum(es.time) / es.qty

    def avg_nodes(self, thr):
        es = self.execs[thr]
        return sum(es.nodes) / es.qty

    def __str__(self):
        return self.name + ': ' + '\n'.join([str(x) for x in self.execs.values()])

    def compute(self, t, n):
        return [x.compute(t, n) for x in self.execs.values()]


def find_avg_time_nodes(wls, threads=1):
    for (wl, res) in wls.items():
        return res.avg_time(threads), res.avg_nodes(threads)
    return None


def find_avg_baseline(file):
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
    return {k: int(v[0] / v[1]) for k, v in name_to_sum_qnty.items()}['hmq4']



def parse_results(names_and_files, baseline_file):
    execs = []
    for name, file in names_and_files:
        print(file)
        rows = np.recfromcsv(file, delimiter=',', case_sensitive=False, deletechars='', encoding='utf-8')
        wl_exec = None
        for row in rows:
            try:
                time = int(row[0])
                wl = row[1]
                nodes = int(row[2])
                threads = int(row[3])
                if not wl or not wl[0].isalpha():
                    print("Invalid row: ", row)
                    continue
            except ValueError:
                print("Invalid row: ", row)
                continue
            if wl_exec is None:
                wl_exec = WlExec(wl)
            elif wl_exec.name != wl:
                raise "Execution file should contain only one worklist"
            wl.add_exec(threads, time, nodes)
        execs.append(name, wl_exec)
    (t, n) = find_avg_baseline(baseline_file)
    return [(name, wl_exec.compute(t, n)) for (name, wl_exec) in execs]

cool_cols = [
    '#386cb0',
    '#67a9cf',
    '#ff7f00',
    '#0020a3',
    '#33a02c',
    '#01665e',
    '#998ec3',
    '#ae362b',
    '#a6cee3',
]


markers = ['o', '^', 'v', 'x',  '*']
linestyles = ['--', '-.']

lines = []
for i in range(len(cool_cols * 10)):
    lines.append(Line2D([0],[0], color=cool_cols[i % len(cool_cols)], lw=2))

def draw_plot_for_wls(time, wl_files, ax, threads, baseline_file):
    ax.set_xscale('log', base=2)
    ax.xaxis.set_major_formatter(tckr.FormatStrFormatter('%0.f'))
    ax.grid(linewidth='0.5', color='lightgray')
    results = parse_results(wl_files, baseline_file)
    for nice_name, result, i in enumerate(results):
        if time:
            print(nice_name)
        if time:
            x = [z['time'] for z in result]
            err = [z['timestd'] for z in result]
        else:
            x = [z['nodes'] for z in result]
            err = [z['nodesstd'] for z in result]
            print(x)
        cur_col = cool_cols[i % len(cool_cols)]
        line = linestyles[i % len(linestyles)]
        ax.errorbar(threads, x, yerr=err, color=cur_col,
                    markersize=6, marker=markers[i % len(markers)], label=name, linestyle=line, lw=1.2)
        i += 1
    plt.ylim(0)
    ax.axhline(y=1, c='#696969')
