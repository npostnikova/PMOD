from plots.draw_plots_helper import draw_plots_for_appendix
import sys

leg_cols=4
name ='plots_all'
timet1=[10, 20, 30, 40, 50, 60, 70]
timet2=[20, 40, 60, 80, 100, 112,]
nodet1=[1, 2, 4, 6, 8]
nodet2=[1, 2, 4, 6]


timet3=[5, 10, 15, 20, 25, 30, 35,]
nodet3=[1, 3, 6, 9]
height=4

threads = [int(t) for t in sys.argv[1:]]

algo_graph = [('sssp', 'twi'), ('sssp', 'web'), ('bfs', 'twi'), ('bfs', 'web')]
titles = ['SSSP TWITTER', 'SSSP WEB', 'BFS TWITTER', 'BFS WEB']

draw_plots_for_appendix(f'{name}1', algo_graph, titles, threads,
                        fig_height=height, fig_width=2.7, time_ticks=timet2,
                        col_num=leg_cols, nodes_min=0.5, nodes_max=5, nodes_ticks=nodet2)



algo_graph = [('sssp', 'usa'), ('astar', 'usa'), ('bfs', 'usa'), ('bfs', 'west')]

titles = ['SSSP USA', 'A* USA', 'BFS USA', 'BFS WEST']


draw_plots_for_appendix(f'{name}2', algo_graph, titles, threads,
                        fig_height=height, fig_width=2.7, time_ticks=timet1,
                        col_num=leg_cols, nodes_min=0.5, nodes_max=5, nodes_ticks=nodet1, with_legend=True)

algo_graph = [('sssp', 'west'), ('astar', 'west'), ('boruvka', 'west'), ('boruvka', 'usa')]

titles = ['SSSP WEST', 'A* WEST', 'MST WEST', 'MST USA']

draw_plots_for_appendix(f'{name}3', algo_graph, titles, threads,
                        fig_height=height, fig_width=2.7, time_ticks=timet3,
                        col_num=leg_cols, nodes_min=0.5, nodes_max=5, nodes_ticks=nodet3, with_legend=True)

