import sys
import csv
import argparse
from pathlib import Path


def generate_amqs(graph_type,
                  wl,
                  algo="bfs",
                  C=None,
                  runs_number=3):
    algo_to_elemets_type = { "bfs": "WorkItem",
                             "sssp": "UpdateRequest",
                             "astar": "UpdateRequest",
                             "pagerank": "WorkItem",
                             "boruvka": "WorkItem"
                             }
    header = open(f"/home/ubuntu/PMOD/Galois-2.2.1/apps/{algo}/Heatmaps.h", "w")
    script = open(f"/home/ubuntu/PMOD/heatmaps/heatmap_run_{algo}_{graph_type}_{wl}.sh", "w")
    for p in range(11):
        for q in range(11):
            if wl == "smq":
                suff = f"1_{2 ** p}_{2 ** q}"
                amq_params = [
                    algo_to_elemets_type[algo],  # Type of the task
                    "Comparer" if algo != "boruvka" else "seq_gt",  # Tasks comparer
                    f"Prob<1, {2 ** p}>",  # Steal probability
                    "true",  # Concurrent
                    str(2 ** q)  # Steal batch size
                ]
                header.write(
                    f"\ttypedef StealingMultiQueue<{', '.join(amq_params)}> "
                    f"smqhm_{suff};\n"
                )
            elif wl == "amq":
                suff = f"1_{2 ** p}_1_{2 ** q}"
                amq_params = [
                    algo_to_elemets_type[algo],  # Type of the task
                    "Comparer" if algo != "boruvka" else "seq_gt",  # Tasks comparer
                    "2",  # C
                    "false",  # Decrease key todo: deprecated
                    "void",  # Decrease key indexer todo: deprecated
                    "true",  # Concurrent
                    "false",  # Blocking
                    f"Prob<1, {2 ** p}>",  # Push probability
                    f"Prob<1, {2 ** q}>",  # Pop probability
                    "int" if algo == "pagerank" else "unsigned long",  # Priority type
                ]
                header.write(
                    f"\ttypedef AdaptiveMultiQueue<{', '.join(amq_params)}> "
                    f"amqhm_{suff};\n"
                )
            elif wl == "slamq":
                suff = f"1_{2 ** p}_{2 ** q}"
                amq_params = [
                    algo_to_elemets_type[algo],  # Type of the task
                    "Comparer" if algo != "boruvka" else "seq_gt",  # Tasks comparer
                    str(C),
                    f"Prob<1, {2 ** p}>",  # Steal probability
                    "true",  # Concurrent
                    str(2 ** q)  # Steal batch size
                ]
                header.write(
                    f"\ttypedef SkipListAMQ<{', '.join(amq_params)}> "
                    f"slamqhm_{suff};\n"
                )
            header.write(f"\tif (wl == \"{wl}hm_{suff}\")\n")
            if algo == "bfs" or algo == "sssp":
                if algo == "bfs":
                    header.write(
                        "\t\tGalois::for_each(WorkItem(source, 1), Process(graph), "
                        f"Galois::wl<{wl}hm_{suff}>());\n")
                else:
                    header.write(
                        "\t\tGalois::for_each_local(initial, Process(this, graph), "
                        f"Galois::wl<{wl}hm_{suff}>());\n")

                for k in range(runs_number):
                    if graph_type == "twitter":
                        script.write(
                            f"/home/ubuntu/PMOD/Galois-2.2.1/build/apps/{algo}/{algo} "
                            "~/PMOD/datasets/GAP-twitter.bin "
                            f"-wl={wl}hm_{suff} -t=96 -startNode=14 "
                            f"-reportNode=15 -resultFile={algo}_twitter_{wl}_96\n")
                    elif graph_type == "web":
                        script.write(
                            f"/home/ubuntu/PMOD/Galois-2.2.1/build/apps/{algo}/{algo} "
                            "~/PMOD/datasets/GAP-web.bin "
                            f"-wl={wl}hm_{suff} -t=96 -startNode=5900 "
                            f"-reportNode=59001 -resultFile={algo}_{graph_type}_{wl}_96\n")
                    elif graph_type == "ctr":
                        script.write(
                            f"/home/ubuntu/PMOD/Galois-2.2.1/build/apps/{algo}/{algo} "
                            "~/PMOD/datasets/USA-road-dCTR.bin "
                            f"-wl={wl}hm_{suff} -t=96 -resultFile={algo}_ctr_{wl}_96\n")
                    elif graph_type == "usa":
                        script.write(
                            f"/home/ubuntu/PMOD/Galois-2.2.1/build/apps/{algo}/{algo} "
                            "~/PMOD/datasets/USA-road-dUSA.bin "
                            f"-wl={wl}hm_{suff} -t=96 -resultFile={algo}_usa_{wl}_96\n")
                    elif graph_type == "lj":
                        script.write(
                            f"/home/ubuntu/PMOD/Galois-2.2.1/build/apps/{algo}/{algo} "
                            "~/PMOD/datasets/soc-LiveJournal1.bin "
                            f"-wl={wl}hm_{suff} -t=96 -resultFile={algo}_lj_{wl}_96\n")
                    else:
                        raise UnsupportedGraphType(graph_type)

            elif algo == "pagerank":
                header.write(
                    "\t\tGalois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),\n"
                    "\t\t\tboost::make_transform_iterator(graph.end(), std::ref(fn)),\n"
                    f"\t\t\tProcess(graph, tolerance, amp), Galois::wl<{wl}hm_{suff}>());\n")
                if graph_type == "twitter":
                    graph_file = "GAP-twitter.bin"
                elif graph_type == "web":
                    graph_file = "GAP-web.bin"
                else:
                    raise UnsupportedGraphType(graph_type)
                for k in range(runs_number):
                    script.write(
                        f"/home/ubuntu/PMOD/Galois-2.2.1/build/apps/{algo}/{algo} "
                        f"~/PMOD/datasets/{graph_file} "
                        f"-wl={wl}hm_{suff} -t=96 -algo async_prt "
                        f"-graphTranspose ~/PMOD/datasets/{graph_file}_transpose "
                        "-amp 1000 -tolerance 0.01 "
                        f"-resultFile={algo}_{graph_type}_{wl}_96\n")

            elif algo == "astar":
                header.write(
                    "\t\tGalois::for_each_local(initial, Process(this, graph), "
                    f"Galois::wl<{wl}hm_{suff}>());\n")

                for k in range(runs_number):
                    if graph_type == "usa":
                        graph_file_pref = "USA-road-dUSA"
                        script.write(
                            f"/home/ubuntu/PMOD/Galois-2.2.1/build/apps/{algo}/{algo} "
                            f"~/PMOD/datasets/{graph_file_pref}.bin "
                            f"-wl={wl}hm_{suff} -t=96 "
                            f"-coordFilename ~/PMOD/datasets/{graph_file_pref}.co "
                            f"-resultFile={algo}_{graph_type}_{wl}_96\n")
                            # todo start and report nodes
                else:
                    raise UnsupportedGraphType(graph_type)

            elif algo == "boruvka":
                header.write(
                    "\t\tGalois::for_each_local(initial, process(), "
                    f"Galois::wl<{wl}hm_{suff}>());\n")

                for k in range(runs_number):
                    script.write(
                        f"/home/ubuntu/PMOD/Galois-2.2.1/build/apps/{algo}/{algo}-merge "
                        "~/PMOD/datasets/USA-road-dUSA.bin "
                        f"-wl={wl}hm_{suff} -t=96 -resultFile={algo}_{graph_type}_{wl}_96\n")
            else:
                raise UnsupportedAlgo(algo)
    header.close()
    script.close()
    script.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--algo',
                        help='Supported algos: bfs, sssp, pagerank, astar')
    parser.add_argument('--graph',
                        help='Supported graphs: twitter, ctr, lj, usa, web')
    parser.add_argument('--runs', type=int,
                        help='Number of runs', default=5)
    parser.add_argument('--C', type=int,
                        help='Just C', default=2)
    parser.add_argument('--wl',
                        help='Supported wls: amq, smq')
    args = parser.parse_args()
    generate_amqs(args.graph, args.wl, args.algo, args.runs)

