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

print(find_best_name(sys.argv[1]))