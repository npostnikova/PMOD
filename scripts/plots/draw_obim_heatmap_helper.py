from plots.obim_heatmap import draw_heatmap_obim

def draw_hm_obim(image_filename, wl, path_prefix, threads, small_delta,
                 xcolticks=[1, 1.5, 2],
                 ycolticks=[1.25, 1.5, 1.75, 2]):
    folder = f'heatmaps/{wl}_heatmaps'
    if small_delta:
        draw_heatmap_obim( image_filename,  [
                f'{path_prefix}/{folder}/sssp_twi_{wl}_{threads}',
                f'{path_prefix}/{folder}/sssp_web_{wl}_{threads}',
                f'{path_prefix}/{folder}/bfs_twi_{wl}_{threads}',
                f'{path_prefix}/{folder}/bfs_web_{wl}_{threads}',
                f'{path_prefix}/{folder}/boruvka_usa_{wl}_{threads}',
                f'{path_prefix}/{folder}/boruvka_west_{wl}_{threads}',
            ],
            [  f'{path_prefix}/baseline/sssp_twi_base',
               f'{path_prefix}/baseline/sssp_web_base',
               f'{path_prefix}/baseline/bfs_twi_base',
               f'{path_prefix}/baseline/bfs_web_base',
               f'{path_prefix}/baseline/boruvka_usa_base',
               f'{path_prefix}/baseline/boruvka_west_base',
            ],
            [  'SSSP TWITTER',
               'SSSP WEB',
               'BFS TWITTER',
               'BFS WEB',
               'MST USA',
               'MST WEST',
           ], small_delta, xcolticks=xcolticks, ycolticks=ycolticks
       )
    else:
        draw_heatmap_obim(image_filename, [
               f'{path_prefix}/{folder}/bfs_usa_{wl}_{threads}',
               f'{path_prefix}/{folder}/bfs_west_{wl}_{threads}',
               f'{path_prefix}/{folder}/sssp_usa_{wl}_{threads}',
               f'{path_prefix}/{folder}/sssp_west_{wl}_{threads}',
               f'{path_prefix}/{folder}/astar_usa_{wl}_{threads}',
               f'{path_prefix}/{folder}/astar_west_{wl}_{threads}',
           ],
           [
               f'{path_prefix}/baseline/bfs_usa_base',
               f'{path_prefix}/baseline/bfs_west_base',
               f'{path_prefix}/baseline/sssp_usa_base',
               f'{path_prefix}/baseline/sssp_west_base',
               f'{path_prefix}/baseline/astar_usa_base',
               f'{path_prefix}/baseline/astar_west_base',
           ],
           [
               'BFS USA',
               'BFS WEST',
               'SSSP USA',
               'SSSP WEST',
               'A* USA',
               'A* WEST',
           ], small_delta, xcolticks=xcolticks, ycolticks=ycolticks
        )

