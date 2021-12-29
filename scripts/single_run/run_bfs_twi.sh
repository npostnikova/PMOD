lj=$DATASETS_DIR/soc-LiveJournal1.bin
lj_tr=$DATASETS_DIR/soc-LiveJournal1.bin_transpose
ctr=$DATASETS_DIR/USA-road-dCTR.bin
usa=$DATASETS_DIR/USA-road-dUSA.bin
usa_coord=$DATASETS_DIR/USA-road-d.USA.co
web=$DATASETS_DIR/GAP-web.bin
web_tr=$DATASETS_DIR/GAP-web.bin_transpose
twi=$DATASETS_DIR/GAP-twitter.bin
twi_tr=$DATASETS_DIR/GAP-twitter.bin_transpose
west=$DATASETS_DIR/USA-road-dW.bin
west_coord=$DATASETS_DIR/USA-road-d.W.co

bfs=$GALOIS_HOME/build/apps/bfs/bfs
sssp=$GALOIS_HOME/build/apps/sssp/sssp
boruvka=$GALOIS_HOME/build/apps/boruvka/boruvka-merge
astar=$GALOIS_HOME/build/apps/astar/astar
pagerank=$GALOIS_HOME/build/apps/pagerank/pagerank
 
algo=bfs
graph=twi
${!algo} ${!graph} -wl $1 -t $2 -resultFile $3 -startNode 14 -reportNode 15 -delta 0
