from plots.heatmap_lib import *

import numpy as np
from matplotlib import cm
import matplotlib
import seaborn as sns
import matplotlib.pylab as pyl
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter
from matplotlib.patches import Rectangle


def draw_heatmap(
        result_image_name,
        input_files,
        baseline_files,
        titles,
        x_prob,
        y_prob,
        xcolticks=[],
        ycolticks=[],
        special_xlabel=None,
        special_ylabel=None
):
    fig, axn = plt.subplots(2, len(input_files), sharey='row')

    file_mul = .32
    cbar_ax = fig.add_axes([ file_mul * len(input_files) + .11, 0.65, .03, .35])
    cbar_ax2 = fig.add_axes([ file_mul * len(input_files) + .11, 0.1, .03, .35])


    ticks = [0.5, 2.5, 4.5, 6.5, 8.5, 10.5]
    labels = ['$2^{0}$', '$2^{2}$', '$2^{4}$', '$2^{6}$', '$2^{8}$', '$2^{10}$']
    labels_neg = ['$2^{0}$', '$2^{-2}$', '$2^{-4}$', '$2^{-6}$', '$2^{-8}$', '$2^{-10}$']

    max_t = 0
    min_t = 1000
    max_n = 0
    min_n = 1000
    best_vals = []

    for i, file in enumerate(input_files):
        (time_hmq, nodes_hmq) = find_avg_hmq(baseline_files[i])
        #print(time_div, nodes_div)
        (hm, hm_n) = heatmap2array(file, time_hmq, nodes_hmq)
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
        fmt = lambda x, pos: '{:.1f}'.format(round(x, 1))

        thm = sns.heatmap(hm, ax=axn[0][i], center=1, cmap=cmap,
                          vmin=min_t, vmax=max_t,
                          linewidths=.7, square=True, cbar_ax=cbar_ax,
                          cbar_kws={
                              'format': FuncFormatter(fmt),
                              'label' : 'Speedup',
                              'ticks' : [min_t, *xcolticks, max_t]
                          }
                          )

        nhm = sns.heatmap(hm_n, ax=axn[1][i], center=1, cmap=cmap2,
                          vmin=min_n, vmax=max_n,
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

        axn[0][i].set_xticks(ticks)
        axn[1][i].set_xticks(ticks)
        axn[0][i].set_xticklabels(labels_neg if x_prob else labels, rotation=30)
        axn[1][i].set_xticklabels(labels_neg if x_prob else labels, rotation=30)


        axn[0][i].set_yticks(ticks)
        axn[1][i].set_yticks(ticks)
        xl = labels_neg if y_prob else labels
        xl[-1] = None
        axn[0][i].set_yticklabels(xl, rotation=30)
        axn[1][i].set_yticklabels(xl, rotation=30)

        if special_xlabel is None:
            axn[0][i].set_xlabel('delete: $p_{change}$' if x_prob else 'delete: batch size', fontsize=12)
        else:
            axn[0][i].set_xlabel(special_xlabel, fontsize=12)
        best_vals.append((str(2 ** max_t_id_i) if not y_prob else f'1/{2 ** max_t_id_i}',
                          str(2 ** max_t_id_j) if not x_prob else f'1/{2 ** max_t_id_j}',
                          '{:.2f}'.format(hm[max_t_id_i][max_t_id_j]),
                          '{:.2f}'.format(hm_n[max_t_id_i][max_t_id_j])))


    fig.axes[-1].yaxis.label.set_size(15)
    fig.axes[-2].yaxis.label.set_size(15)
    for i in range(0, len(input_files)):
        axn[0][i].set_title(titles[i], fontsize=13, pad=17)
    fig.tight_layout(rect=[0, 0, len(input_files) * file_mul + .1, 1.15], pad=0)


    if special_ylabel is None:
        axn[0][0].set_ylabel('insert: $p_{change}$' if y_prob else 'insert: batch size', fontsize=12)
        axn[1][0].set_ylabel('insert: $p_{change}$' if y_prob else 'insert: batch size', fontsize=12)
    else:
        axn[0][0].set_ylabel(special_ylabel, fontsize=12)
        axn[1][0].set_ylabel(special_ylabel, fontsize=12)


    # Write table in latex
    f = open(result_image_name + ".tex", "w")
    f.write("\\begin{table}[h]\n")
    f.write("\\centring\n")
    aaa = ''.join(['|c' for x in titles]) + '|'
    f.write("\\begin{tabular}{ |c" + aaa + " }\n")
    f.write("\\hline\n")
    f.write(' & ' + ' & '.join([f'\\large{{\\textbf{{{t[0:len(t)-3]}}}}}' if t.endswith('WEST') else f'\\large{{\\textbf{{{t}}}}}'  for t in titles]) + ' \\\\\n')
    rrr=['\insprob{} & ' if y_prob else '\insbatch{} & ',
         '\delprob{} & ' if x_prob else '\delbatch{} & ',
         '\\speed{} & ',
         '\workinc{} & ']
    fs = open('mq_best_parameters.csv', 'a')
    for i in range(4):
        f.write("\\hline\n")
        f.write(rrr[i] + ' & '.join([str(x[i]) for x in best_vals]) + ' \\\\\n')
    fs.write(result_image_name.split('_')[0] + ',' + ','.join([str(x[2]) for x in best_vals]) + '\n')
    fs.close()
    f.write("\\hline\n")
    f.write("\\end{tabular}\n")
    f.write("\\vspace{0.3em}\n")
    f.write("\\caption{TODO }\n")
    f.write("\\label{table:todo}\n")
    f.write("\\end{table}\n")
    f.close()
    plt.subplots_adjust(hspace=0)
    plt.show()
    fig.savefig(result_image_name + ".png", bbox_inches="tight", transparent=True)
