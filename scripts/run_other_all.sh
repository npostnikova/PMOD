set -e

~/bfs_build.sh
~/sssp_build.sh
~/mst_build.sh
~/pr_build.sh
~/astar_build.sh

lj=/home/ubuntu/PMOD/datasets/soc-LiveJournal1.bin
lj_tr=/home/ubuntu/PMOD/datasets/soc-LiveJournal1.bin_transpose
ctr=/home/ubuntu/PMOD/datasets/USA-road-dCTR.bin
usa=/home/ubuntu/PMOD/datasets/USA-road-dUSA.bin
usa_coord=/home/ubuntu/PMOD/datasets/USA-road-d.USA.co
web=/home/ubuntu/PMOD/datasets/GAP-web.bin
web_tr=/home/ubuntu/PMOD/datasets/GAP-web.bin_transpose
twi=/home/ubuntu/PMOD/datasets/GAP-twitter.bin
twi_tr=/home/ubuntu/PMOD/datasets/GAP-twitter.bin_transpose
west=/home/ubuntu/PMOD/datasets/USA-road-dW.bin
west_coord=/home/ubuntu/PMOD/datasets/USA-road-d.W.co


bfs=/home/ubuntu/PMOD/Galois-2.2.1/build/apps/bfs/bfs
sssp=/home/ubuntu/PMOD/Galois-2.2.1/build/apps/sssp/sssp
boruvka=/home/ubuntu/PMOD/Galois-2.2.1/build/apps/boruvka/boruvka-merge
astar=/home/ubuntu/PMOD/Galois-2.2.1/build/apps/astar/astar
pagerank=/home/ubuntu/PMOD/Galois-2.2.1/build/apps/pagerank/pagerank

THREADS=( 1 2 4 8 16 32 64 96 )
WLS=( swarm heapswarm spraylist hmq2 hmq3 mq2 mq3 )

runs=10

algo=bfs
for graph in ctr lj
do
  for wl in "${WLS[@]}"
  do
    for t in "${THREADS[@]}"
    do
      for run in $(seq 1 $runs)
      do
        ${!algo} ${!graph} -wl $wl -t $t -resultFile "${algo}_${graph}"
      done
    done
  done
done

algo=sssp
for graph in ctr lj
do
  for wl in "${WLS[@]}"
  do
    for t in "${THREADS[@]}"
    do
      for run in $(seq 1 $runs)
      do
        ${!algo} ${!graph} -wl $wl -t $t -resultFile "${algo}_${graph}"
      done
    done
  done
done


algo=astar
graph=west
for wl in "${WLS[@]}"
do
  for t in "${THREADS[@]}"
  do
    for run in $(seq 1 $runs)
    do
      eval coord=\$$"${graph}_coord"
      ${!algo} ${!graph} -wl $wl -t $t -resultFile "${algo}_${graph}" -coordFilename $coord -startNode 1 -destNode 5639706  -reportNode 5639706
    done
  done
done

algo=boruvka
for graph in west
do
  for wl in "${WLS[@]}"
  do
    for t in "${THREADS[@]}"
    do
      for run in $(seq 1 $runs)
      do
        ${!algo} ${!graph} -wl $wl -t $t -resultFile "${algo}_${graph}"
      done
    done
  done
done

runs=5

algo=pagerank
for graph in lj
do
  for wl in "${WLS[@]}"
  do
    for t in "${THREADS[@]}"
    do
      for run in $(seq 1 $runs)
      do
        eval tr=\$$"${graph}_tr"
        ${!algo} ${!graph} -wl $wl -t $t -resultFile "${algo}_${graph}_1000" -graphTranspose $tr -amp 1000 -tolerance 0.01
      done
    done
  done
done

algo=pagerank
for graph in lj
do
  for wl in "${WLS[@]}"
  do
    for t in "${THREADS[@]}"
    do
      for run in $(seq 1 $runs)
      do
        eval tr=\$$"${graph}_tr"
        ${!algo} ${!graph} -wl $wl -t $t -resultFile "${algo}_${graph}_1" -graphTranspose $tr -amp 1 -tolerance 0.01
      done
    done
  done
done
