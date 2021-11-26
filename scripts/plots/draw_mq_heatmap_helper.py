import mq_heatmap

def draw_hm(
        image_filename,
        path_prefix,
        wl,
        threads,
        x_prob,
        y_prob,
        xcolticks=[[1, 1.5, 2], [1, 1.5, 2]],
        ycolticks=[[1.25, 1.5, 1.75, 2], [1.25, 1.5, 1.75, 2]],
        special_xlabel=None,
        special_ylabel=None,
):
    hm_folder = f'heatmaps/{wl}_heatmaps'
    input_files = [
        f'{path_prefix}/{hm_folder}/sssp_usa_{wl}_{threads}',
        f'{path_prefix}/{hm_folder}/sssp_west_{wl}_{threads}',
        f'{path_prefix}/{hm_folder}/sssp_twi_{wl}_{threads}',
        f'{path_prefix}/{hm_folder}/sssp_web_{wl}_{threads}',
        f'{path_prefix}/{hm_folder}/bfs_usa_{wl}_{threads}',
        f'{path_prefix}/{hm_folder}/bfs_west_{wl}_{threads}',
        f'{path_prefix}/{hm_folder}/bfs_twi_{wl}_{threads}',
        f'{path_prefix}/{hm_folder}/bfs_web_{wl}_{threads}',
        f'{path_prefix}/{hm_folder}/astar_usa_{wl}_{threads}',
        f'{path_prefix}/{hm_folder}/astar_west_{wl}_{threads}',
        f'{path_prefix}/{hm_folder}/boruvka_usa_{wl}_{threads}',
        f'{path_prefix}/{hm_folder}/boruvka_west_{wl}_{threads}',
    ]
    baseline = [
        f'{path_prefix}/baseline/sssp_usa_base',
        f'{path_prefix}/baseline/sssp_west_base',
        f'{path_prefix}/baseline/sssp_twi_base',
        f'{path_prefix}/baseline/sssp_web_base',
        f'{path_prefix}/baseline/bfs_usa_base',
        f'{path_prefix}/baseline/bfs_west_base',
        f'{path_prefix}/baseline/bfs_twi_base',
        f'{path_prefix}/baseline/bfs_web_base',
        f'{path_prefix}/baseline/astar_usa_base',
        f'{path_prefix}/baseline/astar_west_base',
        f'{path_prefix}/baseline/boruvka_usa_base',
        f'{path_prefix}/baseline/boruvka_west_base',
    ]
    titles =   [
        'SSSP USA',
        'SSSP WEST',
        'SSSP TWITTER',
        'SSSP WEB',
        'BFS USA',
        'BFS WEST',
        'BFS TWITTER',
        'BFS WEB',
        'A* USA',
        'A* WEST',
        'MST USA',
        'MST WEST',
    ]
    draw_heatmap(f'{image_filename}1',
              input_files[:6],
              baseline[:6],
              titles[:6],
              x_prob, y_prob,
              xcolticks[0], ycolticks[0],
              special_xlabel=special_xlabel, special_ylabel=special_ylabel
              )
    draw_heatmap(f'{image_filename}2',
              input_files[6:],
              baseline[6:],
              titles[6:],
              x_prob, y_prob,
              xcolticks[1], ycolticks[1],
              special_xlabel=special_xlabel, special_ylabel=special_ylabel
    )

