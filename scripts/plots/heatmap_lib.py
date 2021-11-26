"""A lame library for drawing heatmaps."""

import csv


def heatmap2avg(filename):
    """Computes avg time and work for each worklist in the file.

    Args:
        filename (str): path to the file with execution results.
    Returns:
        Dict[str, (float, float)]: worklist name to avg time and work.
    """

    name_to_sum_qnty = {}
    with open(filename, 'r', newline='') as csvfile:
        reader = csv.DictReader(csvfile, fieldnames=['time', 'wl', 'nodes', 'threads'])
        for row in reader:
            time = int(row['time'])
            name = row['wl']
            nodes = int(row['nodes'])
            cur_time = time
            cur_nodes = nodes
            cur_qty = 1
            if name in name_to_sum_qnty:
                (prev_time, prev_nodes, prev_qty) = name_to_sum_qnty[name]
                cur_time += prev_time
                cur_nodes += prev_nodes
                cur_qty += prev_qty
            name_to_sum_qnty[name] = (cur_time, cur_nodes, cur_qty)
    return {k: (v[0] / v[2], v[1] / v[2]) for (k, v) in name_to_sum_qnty.items()}


def int22deg(val):
    pow2 = 0
    pow_val = 1
    while val > pow_val:
        pow2 += 1
        pow_val *= 2
    if val != pow_val:
        raise RuntimeError(f"Invalid int22deg input: {val}")
    return pow2


def create_csv_dict(wl, time, nodes):
    split_name = wl.split('_')
    if wl.startswith('smq'):
        return {
            'name': wl,
            'time': time,
            'nodes': nodes,
            'y': int22deg(int(split_name[1])),
            'x': int22deg(int(split_name[2])),
        }


def heatmap2csv(filename, time_div, nodes_div):
    """Writes csv heatmap description corresponding to provided execution results.

    Csv records contain each worklist from the provided execution file with scaled
    avg time and work.
    Args:
        filename  (str): path to the file with execution results
        time_div  (int): scaling parameter for time
        nodes_div (int): scaling parameter for work.
    """

    wl_to_avg = heatmap2avg(filename)
    with open(f'{filename}_hm.csv', 'w', newline='') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=['name', 'time', 'nodes', 'y', 'x'])
        writer.writeheader()
        for (name, avgs) in wl_to_avg.items():
            writer.writerow(
                create_csv_dict(
                    name,
                    round(avgs[0] / time_div, 3),
                    round(avgs[1] / nodes_div, 3)
                )
            )


def get_index(wl, small = True):
    """Returns heatmap indices for worklist depending on its parameters.
    Args:
        wl      (str): worklist name
        small   (bool): for OBIM & PMOD only. Whether the heatmap was executed for small or large deltas.
    """

    split_name = wl.split('_')
    if wl.startswith('smq') or wl.startswith('slsmq'):
        return (
            int22deg(int(split_name[1])),
            int22deg(int(split_name[2]))
        )
    if wl.startswith('mq'):
        return (
            int22deg(int(split_name[2])),
            int22deg(int(split_name[3]))
        )
    if wl.startswith('obim') or wl.startswith('pmod'):
        # 0 2 4 8 10 -- small
        # 10 12 14 16 18 -- large
        delta = int(split_name[2])
        chunk_size = int(split_name[1])
        small_map = { 0 : 0, 2 : 1, 4 : 2, 8 : 3, 10 : 4 }
        if small:
            return (
                small_map[delta],
                int22deg(chunk_size) - 5
            )
        else:
            return (
                int((delta - 10) / 2),
                int22deg(chunk_size) - 5
            )



def heatmap2array(filename, time_div, nodes_div):
    """Converts csv heatmap representation to matrix which is used in Seaborn."""

    wl_to_avg = heatmap2avg(filename)
    hm = np.zeros((11, 11))
    hm_n = np.zeros((11, 11))
    for (name, avgs) in wl_to_avg.items():
        time = avgs[0]
        nodes = avgs[1]
        (x, y) = get_index(name)
        hm[x, y] = time_div / time
        hm_n[x, y] = nodes / nodes_div
    return hm, hm_n


def obimheatmap2array(filename, time_div, nodes_div, small_delta):
    """Converts csv OBIM/PMOD heatmap representation to matrix which is used in Seaborn."""

    wl_to_avg = heatmap2avg(filename)

    hm = np.zeros((5, 5))
    hm_n = np.zeros((5, 5))
    for (name, avgs) in wl_to_avg.items():
        time = avgs[0]
        nodes = avgs[1]
        ind = get_index(name, small_delta)

        if ind is None: continue
        x = ind[0]
        y = ind[1]
        hm[x, y] = time_div / time
        hm_n[x, y] = nodes / nodes_div
    return hm, hm_n

def find_avg_hmq(filename):
    """Computes average time & work for the baseline, which is Heap MQ with C = 4."""

    hmq = 'hmq4'
    time_sum = 0
    nodes_sum = 0
    qty = 0
    with open(filename, 'r', newline='') as csvfile:
        reader = csv.DictReader(csvfile, fieldnames=['time', 'wl', 'nodes', 'threads'])
        for row in reader:
            time = int(row['time'])
            name = row['wl']
            nodes = int(row['nodes'])
            if name == hmq:
                time_sum += time
                nodes_sum += nodes
                qty += 1
    return time_sum / qty, nodes_sum / qty
