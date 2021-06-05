set -e

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

runs=5

# Generate SMQ typedefs
algo=bfs
file="/home/ubuntu/PMOD/Galois-2.2.1/apps/${algo}/Heatmaps.h"
echo "" > $file
for i in $(seq 0 10)
do
  for j in $(seq 0 10)
  do
    prob=$(( 2 ** i ))
    size=$(( 2 ** j ))
    wl="mqplhm_${prob}_${size}"
    echo "typedef MultiQueueProbLocal<element_t, Comparer, ${prob}, ${size}, 2, priority_t> ${wl};" >> $file
    echo "if (worklistname == \"${wl}\")" >> $file
    echo "  Galois::for_each(WorkItem(source, 1), Process(graph), Galois::wl<${wl}>());" >> $file
  done
done

~/bfs_build.sh

for graph in twi
do
  for i in $(seq 0 10)
  do
    for j in $(seq 0 10)
    do
      prob=$(( 2 ** i ))
      size=$(( 2 ** j ))
      wl="mqplhm_${prob}_${size}"
      for run in $(seq 1 $runs)
      do
          ${!algo} ${!graph} -wl $wl -t 96 -resultFile "${algo}_${graph}_hm" -startNode=14 -reportNode=15
      done
    done
  done
done

for graph in web
do
  for i in $(seq 0 10)
  do
    for j in $(seq 0 10)
    do
      prob=$(( 2 ** i ))
      size=$(( 2 ** j ))
      wl="mqplhm_${prob}_${size}"
      for run in $(seq 1 $runs)
      do
          ${!algo} ${!graph} -wl $wl -t 96 -resultFile "${algo}_${graph}_hm"  -startNode=5900 -reportNode=5901
      done
    done
  done
done

# Generate SMQ typedefs for SSSP
#algo=sssp
#file="/home/ubuntu/PMOD/Galois-2.2.1/apps/${algo}/Heatmaps.h"
#echo "" > $file
#for i in $(seq 0 10)
#do
#  for j in $(seq 0 10)
#  do
#    prob=$(( 2 ** i ))
#    size=$(( 2 ** j ))
#    wl="mqplhm_${prob}_${size}"
#    echo "typedef MultiQueueProbLocal<element_t, Comparer, ${prob}, ${size}, 2, priority_t> ${wl};" >> $file
#    echo "if (worklistname == \"${wl}\")" >> $file
#    echo "   Galois::for_each_local(initial, Process(this, graph), Galois::wl<${wl}>());" >> $file
#  done
#done
#
#~/sssp_build.sh
#
#for graph in ctr lj
#do
#  for i in $(seq 0 10)
#  do
#    for j in $(seq 0 10)
#    do
#      prob=$(( 2 ** i ))
#      size=$(( 2 ** j ))
#      wl="mqplhm_${prob}_${size}"
#      for run in $(seq 1 $runs)
#      do
#          ${!algo} ${!graph} -wl $wl -t 96 -resultFile "${algo}_${graph}_hm"
#      done
#    done
#  done
#done

#algo=astar
#file="/home/ubuntu/PMOD/Galois-2.2.1/apps/${algo}/Heatmaps.h"
#echo "" > $file
#for i in $(seq 0 10)
#do
#  for j in $(seq 0 10)
#  do
#    prob=$(( 2 ** i ))
#    size=$(( 2 ** j ))
#    wl="mqplhm_${prob}_${size}"
#    echo "typedef MultiQueueProbLocal<element_t, Comparer, ${prob}, ${size}, 2, priority_t> ${wl};" >> $file
#    echo "if (worklistname == \"${wl}\")" >> $file
#    echo "   Galois::for_each_local(initial, Process(this, graph), Galois::wl<${wl}>());" >> $file
#  done
#done
#
#~/astar_build.sh
#
#for graph in west
#do
#  for i in $(seq 0 10)
#  do
#    for j in $(seq 0 10)
#    do
#      prob=$(( 2 ** i ))
#      size=$(( 2 ** j ))
#      wl="mqplhm_${prob}_${size}"
#      for run in $(seq 1 $runs)
#      do
#        eval coord=\$$"${graph}_coord"
#        ${!algo} ${!graph} -wl $wl -t 96 -resultFile "${algo}_${graph}_hm" -coordFilename $coord -delta 14 -startNode 1 -destNode 5639706  -reportNode 5639706
#      done
#    done
#  done
#done
#
#algo=boruvka
#file="/home/ubuntu/PMOD/Galois-2.2.1/apps/${algo}/Heatmaps.h"
#echo "" > $file
#for i in $(seq 0 10)
#do
#  for j in $(seq 0 10)
#  do
#    prob=$(( 2 ** i ))
#    size=$(( 2 ** j ))
#    wl="mqplhm_${prob}_${size}"
#    echo "typedef MultiQueueProbLocal<WorkItem, seq_gt, ${prob}, ${size}, 2, priority_t> ${wl};" >> $file
#    echo "if (worklistname == \"${wl}\")" >> $file
#    echo "   Galois::for_each_local(initial, process(), Galois::wl<${wl}>());" >> $file
#  done
#done
#
#~/mst_build.sh
#
#for graph in west
#do
#  for i in $(seq 0 10)
#  do
#    for j in $(seq 0 10)
#    do
#      prob=$(( 2 ** i ))
#      size=$(( 2 ** j ))
#      wl="mqplhm_${prob}_${size}"
#      for run in $(seq 1 $runs)
#      do
#        eval coord=\$$"${graph}_coord"
#        ${!algo} ${!graph} -wl $wl -t 96 -resultFile "${algo}_${graph}_hm"
#      done
#    done
#  done
#done

algo=pagerank
file="/home/ubuntu/PMOD/Galois-2.2.1/apps/${algo}/Heatmaps.h"
echo "" > $file
for i in $(seq 0 10)
do
  for j in $(seq 0 10)
  do
    prob=$(( 2 ** i ))
    size=$(( 2 ** j ))
    wl="mqplhm_${prob}_${size}"
    echo "typedef MultiQueueProbLocal<WorkItem, Comparer, ${prob}, ${size}, 2, priority_t> ${wl};" >> $file
    echo "if (worklistname == \"${wl}\")" >> $file
    echo "   Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn))," >> $file
    echo "     boost::make_transform_iterator(graph.end(), std::ref(fn))," >> $file
    echo "     Process(graph, tolerance, amp), Galois::wl<${wl}>());" >> $file
  done
done

#~/pr_build.sh

for graph in lj
do
  for i in $(seq 0 10)
  do
    for j in $(seq 0 10)
    do
      prob=$(( 2 ** i ))
      size=$(( 2 ** j ))
      wl="mqplhm_${prob}_${size}"
      for run in $(seq 1 $runs)
      do
        eval tr=\$$"${graph}_tr"
        ${!algo} ${!graph} -wl $wl -t $t -resultFile "${algo}_${graph}_10" -graphTranspose $tr -amp 10 -tolerance 0.01
     done
    done
  done
done

