algo=$1
graph=$2
wl=$3
threads=$4
times=$([ -z $5 ] && echo 5 || echo $5)
result=$([ -z $6 ] && echo "${algo}_${graph}" || echo "${algo}_${graph}_$6")

$MQ_ROOT/scripts/build/build.sh $algo

for in $(seq 0 $times); do
  $MQ_ROOT/scripts/single_run/run_${algo}_${graph}.sh $wl $threads $result
done
