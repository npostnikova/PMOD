algo=$1
graph=$2
wl=$3
times=$([ -z $4 ] && echo 5 || echo $4)
result=$([ -z $5 ] && echo "${algo}_${graph}" || echo "${algo}_${graph}_$5")

$MQ_ROOT/scripts/build/build.sh $algo

for threads in 1 2 4 8 16 32 64 128 256; do
  for i in $(seq 1 $times); do
    $MQ_ROOT/scripts/single_run/run_${algo}_${graph}.sh $wl $threads $result
  done
done
