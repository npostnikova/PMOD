from plots.draw_plots_helper import draw_plots_for_appendix

leg_cols=4
name ='plots_all'
timet1=[10, 20, 30, 40, 50, 60, 70]
timet2=[20, 40, 60, 80, 100, 112,]
nodet1=[1, 2, 4, 6, 8]
nodet2=[1, 2, 4, 6]


timet3=[5, 10, 15, 20, 25, 30, 35,]
nodet3=[1, 3, 6, 9]
height=4

algo_graph = [('sssp', 'twi'), ('sssp', 'web'), ('bfs', 'twi'), ('bfs', 'web')]
titles = ['SSSP TWITTER', 'SSSP WEB', 'BFS TWITTER', 'BFS WEB']

draw_plots_for_appendix(f'{name}1',
                        algo_graph, titles, fig_height=height, fig_width=2.7, time_ticks=timet2,
                        colNum=leg_cols, nodes_min=0.5, nodes_max=5, nodes_ticks=nodet2)



algo_graph = [('sssp', 'usa'), ('astar', 'usa'), ('bfs', 'usa'), ('bfs', 'west')]

titles = ['SSSP USA', 'A* USA', 'BFS USA', 'BFS WEST']


draw_plots_for_appendix(f'{name}2',
                        algo_graph,titles, fig_height=height, fig_width=2.7, time_ticks=timet1,
                        colNum=leg_cols, nodes_min=0.5, nodes_max=5, nodes_ticks=nodet1, forbidden_pref=forb, with_legend=False)

algo_graph = [('sssp', 'west'), ('astar', 'west'), ('boruvka', 'west'), ('boruvka', 'usa')]

titles = ['SSSP WEST', 'A* WEST', 'MST WEST', 'MST USA']

draw_plots_for_appendix(f'{name}3',
                        algo_graph,titles, fig_height=height, fig_width=2.7, time_ticks=timet3,
                        colNum=leg_cols, nodes_min=0.5, nodes_max=5, nodes_ticks=nodet3, forbidden_pref=forb, with_legend=False)

