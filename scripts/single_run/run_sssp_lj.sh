lj=$MQ_ROOT/datasets/soc-LiveJournal1.bin
lj_tr=$MQ_ROOT/datasets/soc-LiveJournal1.bin_transpose
ctr=$MQ_ROOT/datasets/USA-road-dCTR.bin
usa=$MQ_ROOT/datasets/USA-road-dUSA.bin
usa_coord=$MQ_ROOT/datasets/USA-road-d.USA.co
web=$MQ_ROOT/datasets/GAP-web.bin
web_tr=$MQ_ROOT/datasets/GAP-web.bin_transpose
twi=$MQ_ROOT/datasets/GAP-twitter.bin
twi_tr=$MQ_ROOT/datasets/GAP-twitter.bin_transpose
west=$MQ_ROOT/datasets/USA-road-dW.bin
west_coord=$MQ_ROOT/datasets/USA-road-d.W.co

bfs=$GALOIS_HOME/build/apps/bfs/bfs
sssp=$GALOIS_HOME/build/apps/sssp/sssp
boruvka=$GALOIS_HOME/build/apps/boruvka/boruvka-merge
astar=$GALOIS_HOME/build/apps/astar/astar
pagerank=$GALOIS_HOME/build/apps/pagerank/pagerank
 
algo=sssp
graph=lj
${!algo} ${!graph} -wl $1 -t $2 -resultFile $3
