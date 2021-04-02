import sys
import csv
from pathlib import Path

from evol_minimization.unknown_amq_params import unknown_params


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
    header = open(output_path / "ApproximationAMQ.h", "w")
    script = open(output_path / f"optimization_run_{algo}_{graph_type}.sh", "w")
    for param in params:
        suff = "_".join([str(v) for v in param.values()])
        print(suff)  # todo remove
        if algo == "bfs":
            amq_params = [
                "WorkItem",  # Type of the task
                "Comparer",  # Tasks comparer
                "2",  # C
                "false",  # Decrease key todo: deprecated
                "void",  # Decrease key indexer todo: deprecated
                "true",  # Concurrent
                "true",  # Blocking
                f"Prob<1, {real_param_val(param, 'pushQ')}>",  # Push probability
                f"Prob<1, {real_param_val(param, 'popQ')}>",  # Pop probability
                "Prob<1, 1>",  # NUMA todo: ignored
                "unsigned long",  # Priority type
                str(real_param_val(param, 's')),
                str(real_param_val(param, 'f')),
                str(real_param_val(param, 'e')),
                str(real_param_val(param, 'segment_size')),
                str(real_param_val(param, 'percent')),
                str(real_param_val(param, 'resume_size'))
            ]
            header.write(
                f"\ttypedef AdaptiveMultiQueue<{', '.join(amq_params)}> "
                f"AMQOPT_{suff};\n"
            )
            header.write(f"\tif (wl == \"amqopt_{suff}\")\n")
            header.write(
                "\t\tGalois::for_each(WorkItem(source, 1), Process(graph), "
                f"Galois::wl<AMQOPT_{suff}>());\n")
            for k in range(runs_number):
                if graph_type == "road":
                    script.write(
                        "/home/ubuntu/PMOD/Galois-2.2.1/build/apps/bfs/bfs "
                        "~/PMOD/datasets/USA-road-dCTR.bin "
                        f"-wl=amqopt_{suff} -t=96 -resultFile=bfs_ctr_96\n")
                elif graph_type == "web":
                    script.write(
                        "/home/ubuntu/PMOD/Galois-2.2.1/build/apps/bfs/bfs "
                        "~/PMOD/datasets/soc-LiveJournal1.bin "
                        f"-wl=amqopt_{suff} -t=96 -resultFile=bfs_lj_96\n")
                else:
                    raise UnsupportedGraphType(graph_type)
        else:
            raise UnsupportedAlgo(algo)
    header.close()
    script.close()


if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Provide the number of algo, graph type and the output file path")
        print("Supported algos: bfs")
        print("Supported graph types: road, web")
        exit()
    algo = sys.argv[1]
    graph_type = sys.argv[2]
    output_path = sys.argv[3]
    runs_number = 3 if len(sys.argv) == 4 else int(sys.argv[4])
    params = read_param_sets()
    generate_amqs(params, algo, graph_type, Path(output_path), runs_number)
