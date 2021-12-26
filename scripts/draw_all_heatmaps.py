from plots.draw_obim_heatmap_helper import draw_hm_obim
from  plots.draw_mq_heatmap_helper import draw_hm

import os

threads = os.environ['HM_THREADS']
mq_root = os.environ['MQ_ROOT']
cpu = os.environ['CPU']
hm_prefix = f'{mq_root}/experiments/{cpu}/'


###### StealingMultiQueue

draw_hm('smq_heatmaps', hm_prefix, 'smq', threads, False, True,
        special_xlabel="STEAL_SIZE", special_ylabel="$p_{steal}$",
        xcolticks=[[1, 2], [1, 2]], ycolticks=[ [3, 6, 9, 12], [4, 8, 12]])

draw_hm('slsmq_heatmaps', hm_prefix, 'slsmq', threads, False, True,
        special_xlabel="STEAL_SIZE", special_ylabel="$p_{steal}$",
        xcolticks=[[0.5, 1, 1.5], [1, 2]], ycolticks=[ [3, 6, 9, 12], [4, 8, 12]])


###### MultiQueue Optimized

# draw_hm('mqpp_heatmaps', hm_prefix, 'mqpp', 128, True, True,
#         xcolticks=[[1, 1.5], [1, 2, 3]], ycolticks=[ [2, 4, 6, 9, 12], [2, 4, 8, 12]])

draw_hm('mqpl_heatmaps', hm_prefix, 'mqpl', threads, False, True,
        xcolticks=[[1, 2], [1, 2, 3]], ycolticks=[ [10, 30, 50], [10, 30, 50]])

# draw_hm('mqlp_heatmaps', hm_prefix, 'mqlp', threads, True, False,
#         xcolticks=[[1, 1.5], [1, 2]], ycolticks=[ [4, 8], [3, 6]])
#
# draw_hm('mqll_heatmaps', hm_prefix, 'mqll', threads, False, False,
#         xcolticks=[[1, 2], [1, 2, 3]], ycolticks=[ [10, 30, 50], [10, 30, 50]])


##### OBIM & PMOD

draw_hm_obim('obim_heatmaps_small', 'obim', hm_prefix, threads, True,
             xcolticks=[2, 4], ycolticks=[2, 4, 6])

draw_hm_obim('obim_heatmaps_large', 'obim', hm_prefix, threads, False,
             xcolticks=[0.5, 1, 1.5], ycolticks=[4, 8])

draw_hm_obim('pmod_heatmaps_small', 'pmod', hm_prefix, threads, True,
             xcolticks=[2, 4], ycolticks=[2, 4, 6])

draw_hm_obim('pmod_heatmaps_large', 'pmod', hm_prefix, threads, False,
             xcolticks=[0.5, 1, 1.5], ycolticks=[7, 14, 21])

