import os
import matplotlib.ticker as tckr
import matplotlib.ticker as mticker
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
import numpy as np
from plots.plots_lib import draw_plot_for_wls

def get_files_by_algo(algo, graph):
    mq_root = os.environ['MQ_ROOT']
    cpu = os.environ['CPU']
    path_prefix = f'{mq_root}/experiments/{cpu}/plots/'
    return [
        ('SMQ (Tuned)', f'{path_prefix}/smq_plots/{algo}_{graph}_smq'),
        ('SMQ (Default)', f'{path_prefix}/smq_plots/{algo}_{graph}_smq_default'),
        ('SkipList SMQ (Tuned)', f'{path_prefix}/slsmq_plots/{algo}_{graph}_slsmq'),
        ('MQ Optimized (Tuned)', f'{path_prefix}/mqpl_plots/{algo}_{graph}_mqpl'),
        ('OBIM (Tuned)', f'{path_prefix}/obim_plots/{algo}_{graph}_obim'),
        ('OBIM (Default)', f'{path_prefix}/obim_plots/{algo}_{graph}_obim_default'),
        ('PMOD (Tuned)', f'{path_prefix}/pmod_plots/{algo}_{graph}_pmod'),
        ('PMOD (Default)', f'{path_prefix}/pmod_plots/{algo}_{graph}_pmod_default'),
        ('RELD', f'{path_prefix}/other_plots/{algo}_{graph}_heapswarm'),
        ('SprayList', f'{path_prefix}/other_plots/{algo}_{graph}_spraylist'),
        ('k-LSM (Tuned)', f'{path_prefix}/klsm_plots/{algo}_{graph}_klsm'),
        # ('Classic MQ', f'{path_prefix}/other_plots/{algo}_{graph}_hmq4'),
    ]

def get_baseline_file(algo, graph):
    mq_root = os.environ['MQ_ROOT']
    cpu = os.environ['CPU']
    # hm_threads = os.environ['HM_THREADS']
    return f'{mq_root}/experiments/{cpu}/baseline/{algo}_{graph}_base_1'


def draw_plots_for_appendix(name, algo_graph,
                            titles, threads, nodes_max, time_ticks,
                            nodes_ticks,fig_height=2, fig_width=2.1,
                            nodes_min=0, with_legend=True, col_num=3
                            ):
    titles = [x.upper() for x in titles]

    fig = plt.figure(figsize=(fig_width * len(algo_graph), fig_height))
    grid = plt.GridSpec(3, len(algo_graph), wspace=0.1, hspace=0.3)

    axarr_n = np.array([])
    axarr_t = np.array([])

    for plot_id, (algo, graph) in enumerate(algo_graph):
        print(f"{algo} {graph}")

        time_subpl = fig.add_subplot(grid[:2, plot_id], yticklabels=[], xticklabels=[])
        nodes_subpl = fig.add_subplot(grid[2, plot_id], yticklabels=[])
        axarr_t = np.append(axarr_t, [time_subpl])
        axarr_n = np.append(axarr_n, [nodes_subpl])
        draw_plot_for_wls(True,  get_files_by_algo(algo, graph), time_subpl, threads, get_baseline_file(algo, graph))
        draw_plot_for_wls(False, get_files_by_algo(algo, graph), nodes_subpl, threads, get_baseline_file(algo, graph))
        time_subpl.set_ylim(ymin=0)
        time_subpl.set_title(titles[plot_id])
        time_subpl.set_xticks(threads)
        nodes_subpl.set_xticks(threads)
        time_subpl.set_xticklabels([''] * len(threads))
        xt = [ '' if i % 2 == 1 else str(x) for i, x in enumerate(threads)]
        nodes_subpl.set_xticklabels(xt)
        nodes_subpl.set_ylim(ymin=nodes_min, ymax=nodes_max)
        # time_subpl.set_ylim(ymin=0, ymax=time_max)
        if not with_legend:
            nodes_subpl.set_xlabel("Threads")

        nodes_subpl.set_yticks(nodes_ticks)
        time_subpl.set_yticks(time_ticks)
        plot_id += 1


    plt.setp(axarr_t[0], ylabel='Speedup')
    plt.setp(axarr_n[0], ylabel='Work\nIncrease')

    axarr_n[0].set_yticklabels(nodes_ticks)
    axarr_t[0].set_yticklabels(time_ticks)

    if with_legend:
        legend_lines, legend_labels = axarr_t[0].get_legend_handles_labels()
        fig.legend(legend_lines, legend_labels, prop={'size': 10},  bbox_to_anchor=(0.5, 1.18),
                   frameon=False, loc='upper center', ncol=col_num)
    fig.tight_layout()
    plt.show()
    fig.savefig(name + ".png", bbox_inches="tight")
