import sys
import csv
import argparse
from pathlib import Path

from evol_minimization.unknown_smq_params import unknown_params


class UnsupportedAlgo(Exception):
    def __init__(self, algo_name):
        self.message = f"The algo {algo_name} is not supported"
        super().__init__(self.message)


class UnsupportedGraphType(Exception):
    def __init__(self, graph_type):
        self.message = f"The graph type {graph_type} is not supported"
        super().__init__(self.message)


params_map = {x.name: x for x in unknown_params}


def read_param_sets(path="next_generation.csv"):
    if path is None:
        path = "next_generation.csv"
    params = []
    if not Path(path).exists():
        return params
    with open(path, "r", newline="") as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            params_vals = {x.name: int(row[x.name]) for x in unknown_params}
            params.append(params_vals)
    return params


def real_param_val(param, name):
    return params_map[name].get_val(param[name])


def generate_amqs(params,
                  algo="bfs",
                  graph_type="road",
                  output_path=Path(),
                  runs_number=3):
    algo_to_element = {
        "bfs": "WorkItem",
        "sssp": "UpdateRequest"
    }
    header = open(output_path / "ApproximationSMQ.h", "w")
    script = open(output_path / f"optimization_run_{algo}_{graph_type}.sh", "w")
    for param in params:
        suff = "_".join([str(v) for v in param.values()])
        print(suff)  # todo remove
        amq_params = [
            algo_to_element[algo],  # Type of the task
            "Comparer",  # Tasks comparer
            str(real_param_val(param, 'stat_buff_size')),
            str(real_param_val(param, 'stat_prob_size')),
            str(real_param_val(param, 'inc_size_per')),
            str(real_param_val(param, 'dec_size_per')),
            str(real_param_val(param, 'inc_prob_per')),
            str(real_param_val(param, 'dec_prob_per')),
            str(real_param_val(param, 'empty_prob_per'))
        ]
        header.write(
            f"typedef StealingMultiQueue<{', '.join(amq_params)}> "
            f"smqopt_{suff};\n"
        )
        header.write(f"if (wl == \"smq_opt_{suff}\")\n")
        if algo == "bfs":
            header.write(
                "\tGalois::for_each(WorkItem(source, 1), Process(graph), "
                f"Galois::wl<smqopt_{suff}>());\n")
            for k in range(runs_number):
                if graph_type == "ctr":
                    script.write(
                        "/home/ubuntu/PMOD/Galois-2.2.1/build/apps/bfs/bfs "
                        "~/PMOD/datasets/USA-road-dCTR.bin "
                        f"-wl=smq_opt_{suff} -t=96 -resultFile=bfs_ctr_96\n")
                elif graph_type == "lj":
                    script.write(
                        "/home/ubuntu/PMOD/Galois-2.2.1/build/apps/bfs/bfs "
                        "~/PMOD/datasets/soc-LiveJournal1.bin "
                        f"-wl=smq_opt_{suff} -t=96 -resultFile=bfs_lj_96\n")
                else:
                    raise UnsupportedGraphType(graph_type)
        elif algo == "sssp":
            header.write(
                "\tGalois::for_each_local(initial, Process(this, graph), "
                f"Galois::wl<smqopt_{suff}>());\n")
            for k in range(runs_number):
                if graph_type == "ctr":
                    script.write(
                        f"/home/ubuntu/PMOD/Galois-2.2.1/build/apps/{algo}/{algo} "
                        "~/PMOD/datasets/USA-road-dCTR.bin "
                        f"-wl=smq_opt_{suff} -t=96 -resultFile={algo}_{graph_type}_96\n")
                elif graph_type == "lj":
                    script.write(
                        f"/home/ubuntu/PMOD/Galois-2.2.1/build/apps/{algo}/{algo} "
                        "~/PMOD/datasets/soc-LiveJournal1.bin "
                        f"-wl=smq_opt_{suff} -t=96 -resultFile={algo}_{graph_type}_96\n")
                else:
                    raise UnsupportedGraphType(graph_type)
        else:
            raise UnsupportedAlgo(algo)
    header.close()
    script.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-g', '--graph',
                        help='Supported graph types: ctr, lj', default=None)
    parser.add_argument('-a', '--algo',
                        help='Supported algorithms: bfs, sssp', default=None)
    parser.add_argument('-o', '--output',
                        help='Output path', default=None)
    parser.add_argument('-n', '--runs',
                        help='Runs number', default=3)
    parser.add_argument('--next',
                        help='Next generation path', default=None)

    args = parser.parse_args()
    params = read_param_sets(args.next)
    generate_amqs(params, args.algo, args.graph, Path(args.output), args.runs)


if __name__ == "__main__":
    main()
