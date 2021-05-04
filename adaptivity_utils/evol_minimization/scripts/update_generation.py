import csv
import argparse
import random
import numpy as np
from pathlib import Path

from evol_minimization.mutation import Individual
from evol_minimization.unknown_smq_params import unknown_params


def read_execution_results(path="execution_results.csv", threads=96):
    wl_to_exec = dict()
    if not Path(path).exists():
        return []
    with open(path, "r", newline="") as csvfile:
        reader = csv.DictReader(
            csvfile,
            fieldnames=["time", "wl", "nodes", "threads"]
        )
        for row in reader:
            wl = row['wl']
            assert int(row['threads']) == threads, \
                f"Number of threads {row['threads']} doesn't match"
            cur_result = (int(row['time']), int(row['nodes']))
            if wl in wl_to_exec:
                wl_to_exec[wl].append(cur_result)
            else:
                wl_to_exec[wl] = [cur_result]

    def params_from_name(name):
        split = name.split("_")[1:]
        return list(map(int, split))

    param_names = [x.name for x in unknown_params]
    return [(Individual(dict(zip(param_names, params_from_name(name)))),
             int(np.average([t for (t, _) in v])),
             int(np.average([n for (_, n) in v])))
            for name, v in wl_to_exec.items()]


def read_prev_generation_with_results(path="active_ancestors.csv"):
    ancestors = []
    if not Path(path).exists():
        return ancestors
    with open(path, "r", newline="") as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            params = {x.name: row[x.name] for x in unknown_params}
            time = int(row["time"])
            nodes = int(row["nodes"])
            ancestors.append((Individual(params), time, nodes))
    return ancestors


# todo scale size
def scale_results(results):
    return [(i, int(t / 1e7), int(n / 1e5)) for (i, t, n) in results]


def append_individuals(individuals, path="all_individuals.csv"):
    exists = Path(path).exists()
    with open(path, "a", newline="") as csvfile:
        writer = csv.DictWriter(
            csvfile,
            fieldnames=[x.name for x in unknown_params] + ['time', 'nodes']
        )
        if not exists:
            writer.writeheader()
        for (ind, t, n) in individuals:
            params = ind.params_vals
            params['time'] = t
            params['nodes'] = n
            writer.writerow(params)


def write_active_ancestors(individuals, path="active_ancestors.csv"):
    with open(path, "w", newline="") as csvfile:
        writer = csv.DictWriter(
            csvfile,
            fieldnames=[x.name for x in unknown_params] + ['time', 'nodes']
        )
        writer.writeheader()
        for (ind, t, n) in individuals:
            params = ind.params_vals
            params['time'] = t
            params['nodes'] = n
            writer.writerow(params)


# TODO params
def combine_next_population(exec_path="execution_results.csv", gen_size=None,
                            best=None):
    if gen_size is None:
        gen_size = 50
    if best is None:
        best = 40
    new = read_execution_results(exec_path)
    new = scale_results(new)
    append_individuals(new)
    prev = read_prev_generation_with_results()
    gens = new + prev
    sorted_gens = sorted(gens, key=lambda ind_info: ind_info[1])  # by time
    ind_set = set()
    next_population = []
    i = 0
    while len(ind_set) < best and i < len(sorted_gens):
        if sorted_gens[i][0] not in ind_set:
            ind_set.add(sorted_gens[i][0])
            next_population.append(sorted_gens[i])
        i += 1
    random.shuffle(new)
    if len(next_population) < gen_size:
        for (ind, t, n) in new:
            if ind not in ind_set:
                ind_set.add(ind)
                next_population.append((ind, t, n))
                if len(next_population) >= gen_size:
                    break
    write_active_ancestors(next_population)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument('--exec_path',
                        help='Path to the file with execution results',
                        default='execution_results.csv')
    parser.add_argument('--gen_size', type=int,
                        help='Size of the generation', default=None)
    parser.add_argument('--highest', type=int,
                        help='Number of elements with the highest priority',
                        default=None)
    args = parser.parse_args()
    combine_next_population(
        args.exec_path,
        args.gen_size,
        args.highest
    )
