import heatmap_lib
import numpy as np
from matplotlib import cm
import matplotlib
import seaborn as sns
import matplotlib.pylab as pyl
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter
from matplotlib.patches import Rectangle


def draw_heatmap_obim(
        result_image_name,
        input_files,
        baseline_files,
        titles,
        small_delta,
        xcolticks=[],
        ycolticks=[],
):
    fig, axn = plt.subplots(2, len(input_files), sharey='row')

    file_mul = .32
    cbar_ax = fig.add_axes([ file_mul * len(input_files) + .11, 0.73, .03, .35])
    cbar_ax2 = fig.add_axes([ file_mul * len(input_files) + .11, 0.17, .03, .35])


    ticks_delta = [0.5, 1.5, 2.5, 3.5]
    ticks_chunks = [0.5, 1.5, 2.5, 3.5, 4.5]
    deltas = [0, 2, 4, 8, 10] if small_delta else [10, 12, 14, 16, 18]
    chunks = ['$2^{5}$', '$2^{6}$', '$2^{7}$', '$2^{8}$', '$2^{9}$']

    max_t = 0
    min_t = 1000
    max_n = 0
    min_n = 1000
    for i, file in enumerate(input_files):
        (time_hmq, nodes_hmq) = find_avg_hmq(baseline_files[i])
        #print(time_div, nodes_div)
        (hm, hm_n) = obimheatmap2array(file, time_hmq, nodes_hmq, small_delta)
        max_t_id_i = 0
        max_t_id_j = 0
        max_t_local = 0
        for k in range(0, len(hm)):
            for j in range(0, len(hm[k])):
                val = hm[k][j]
                if val > max_t:
                    max_t = val
                if val > max_t_local:
                    max_t_local = val
                    max_t_id_i = k
                    max_t_id_j = j
                if val < min_t:
                    min_t = val
                if hm_n[k][j] > max_n:
                    max_n = hm_n[k][j]
                if hm_n[k][j] < min_n:
                    min_n = hm_n[k][j]
        cmap = matplotlib.colors.LinearSegmentedColormap.from_list("", ["#990000", "#ffff00", "#38761d"])
        cmap2 = matplotlib.colors.LinearSegmentedColormap.from_list("", ["#38761d", "#ffff00", "#990000"])


        print(titles[i] + ": {" + str(2 ** max_t_id_j) + ", " + str(2 **max_t_id_i) + "} " + str(hm[max_t_id_i][max_t_id_j]))
        fmt = lambda x,pos: '{:.2f}'.format(x)

        thm = sns.heatmap(hm, ax=axn[0][i], center=1, cmap=cmap,
                          vmin=min_t, vmax=max_t, annot=True, fmt=".1f",
                          linewidths=.7, square=True, cbar_ax=cbar_ax,
                          cbar_kws={
                              'format': FuncFormatter(fmt),
                              'label' : 'Speedup',
                              'ticks' : [min_t, *xcolticks, max_t]
                          }
                          )

        nhm = sns.heatmap(hm_n, ax=axn[1][i], center=1, cmap=cmap2,
                          vmin=min_n, vmax=max_n, annot=True, fmt=".1f",
                          linewidths=.7, square=True, cbar_ax=cbar_ax2,
                          cbar_kws={
                              'format': FuncFormatter(fmt),
                              'label' : 'Work Increase',
                              'ticks' : [min_n, *ycolticks, max_n]
                          }
                          )
        thm.xaxis.set_ticks_position('top')
        nhm.xaxis.set_ticks_position('top')

        thm.add_patch(Rectangle((max_t_id_j, max_t_id_i), 1, 1, fill=False, edgecolor='black', lw=1.2))
        nhm.add_patch(Rectangle((max_t_id_j, max_t_id_i), 1, 1, fill=False, edgecolor='black', lw=1.2))

        axn[0][i].set_xticks(ticks_chunks)
        axn[1][i].set_xticks(ticks_chunks)
        axn[0][i].set_xticklabels(chunks, rotation=30, fontsize=12)
        axn[1][i].set_xticklabels(chunks, rotation=30, fontsize=12)


        axn[0][i].set_yticks(ticks_delta)
        axn[1][i].set_yticks(ticks_delta)
        axn[0][i].set_yticklabels([str(d) for d in deltas])
        axn[1][i].set_yticklabels(deltas)

        fs = 13
        axn[0][i].set_xlabel('chunk size', fontsize=fs)
        axn[1][i].set_xlabel('chunk size', fontsize=fs)

    fig.axes[-1].yaxis.label.set_size(15)
    fig.axes[-2].yaxis.label.set_size(15)
    for i in range(0, len(input_files)):
        axn[0][i].set_title(titles[i], fontsize=13, pad=17)
    fig.tight_layout(rect=[0, 0, len(input_files) * file_mul + .1, 1.3])

    for ax in axn[0]:
        ax.set_yticklabels([str(d) for d in deltas])
    axn[0][0].set_ylabel('delta', fontsize=fs)
    axn[1][0].set_ylabel('delta', fontsize=fs)

    plt.show()
    fig.savefig(result_image_name + ".png", bbox_inches="tight", transparent=True)