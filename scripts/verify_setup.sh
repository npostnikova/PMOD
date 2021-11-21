set -e

source $MQ_ROOT/set_envs.sh

echo "Running sample python script"
$PYTHON_EXPERIMENTS $MQ_ROOT/scripts/python_sample.py

echo "Checking that all benchmarks and graphs work"
echo "On success *_benchmarks_test files should contain one record each"

rm -f *_benchmarks_test

for algo in bfs sssp astar boruvka; do
  $MQ_ROOT/scripts/build/build.sh $algo
done

for algo in bfs sssp; do
  for graph in usa west twi web; do
    $MQ_ROOT/scripts/single_run/run_${algo}_${graph}.sh obim $MAX_CPU_NUM benchmarks_test
  done
done

for algo in astar boruvka; do
  for graph in usa west; do
    $MQ_ROOT/scripts/single_run/run_${algo}_${graph}.sh obim $MAX_CPU_NUM benchmarks_test
  done
done

echo "It's time to check *_benchmarks_test files!"
echo "Each file is expected to have only one line <time>,obim,<nodes>,<threads>"

verify_execution_result() {
  algo=$1
  graph=$2
  echo "${algo}_${graph}_benchmarks_test contents:"
  cat "${algo}_${graph}_benchmarks_test"

  lines=$( wc -l < "${algo}_${graph}_benchmarks_test" )
  if [ $lines -ne 1 ]; then
    echo "Wrong number of lines"
    exit 1
  fi
  line=$( head -n 1 "${algo}_${graph}_benchmarks_test" )
  if ! [[ "$line" =~ ^[0-9]{3,},obim,[0-9]{3,},$MAX_CPU_NUM,^[0-9]{1,2}$ ]]; then
    echo "Invalid execution line"
    exit 1
  fi
}

for algo in bfs sssp; do
  for graph in usa west twi web; do
    verify_execution_result $algo $graph
  done
done

for algo in astar boruvka; do
  for graph in usa west; do
    verify_execution_result $algo $graph
  done
done

echo "Seems like everything is OK!"
echo "Cleaning"

rm -f *_benchmarks_test