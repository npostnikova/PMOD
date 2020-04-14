#!/bin/bash
#
# AdaptiveMultiQueue testing.
# Should be run from build directory.
set -e

BASE_SCRIPTS="$(pwd)/scripts"
if [[ -z "$BASEINPUT" ]]; then
  BASEINPUT="$(pwd)/../inputs"
fi

if [[ -z "$NUMTHREADS" ]]; then
  NUMTHREADS="$(cat /proc/cpuinfo | grep processor | wc -l)"
fi

RESULTDIR="$(pwd)/amq_logs"
REPORTS_DIR="$(pwd)/amq_reports"

if [[ ! -e Makefile ]]; then
  echo "Execute this script from the base of your build directory" 1>&2
  exit 1
fi

SKIPPED=0

run() {
  echo "$1"
  cmd="${@:2} -noverify"
  echo $cmd
  name=$(echo "$1" | sed -e 's/\//_/g')
  echo $name
  logfile="$RESULTDIR/$name.log"
  if [[ ! -e "$logfile"  && -x "$2" ]]; then  
    echo -en '\033[1;31m'
    echo -n "$cmd"
    echo -e '\033[0m'
    echo "Name: $name" >> "$logfile"
    $BASE_SCRIPTS/run.py --threads 1:$NUMTHREADS --timeout $((60*20))  -- $cmd 2>&1 | tee "$logfile"
  else
    echo -en '\033[1;31m'
    echo -n Skipping $1
    echo -e '\033[0m'
    SKIPPED=$(($SKIPPED + 1))
  fi
}

mkdir -p "$RESULTDIR"
# rm $RESULTDIR/*

run bfs_amq_rome99 apps/bfs/bfs -algo=async -wl=adap-mq2 "$BASEINPUT/structured/rome99.gr"
run bfs_hmq_rome99 apps/bfs/bfs -algo=async -wl=heapmultiqueue2 "$BASEINPUT/structured/rome99.gr"
run bfs_amq_torus5 apps/bfs/bfs -algo=async -wl=adap-mq2 "$BASEINPUT/structured/torus5.gr"
run bfs_hmq_torus5 apps/bfs/bfs -algo=async -wl=heapmultiqueue2 "$BASEINPUT/structured/torus5.gr"

run sssp_amq_rome99 apps/sssp/sssp -algo=async -wl=adap-mq2 "$BASEINPUT/structured/rome99.gr"
run sssp_amqdk_rome99 apps/sssp/sssp -algo=async -wl=adap-mq2-dk "$BASEINPUT/structured/rome99.gr"
run sssp_hmq_rome99 apps/sssp/sssp -algo=async -wl=heapmultiqueue2 "$BASEINPUT/structured/rome99.gr"
run sssp_amq_torus5 apps/sssp/sssp -algo=async -wl=adap-mq2 "$BASEINPUT/structured/torus5.gr"
run sssp_amqdk_torus5 apps/sssp/sssp -algo=async -wl=adap-mq2-dk "$BASEINPUT/structured/torus5.gr"
run sssp_hmq_rome99 apps/sssp/sssp -algo=async -wl=heapmultiqueue2 "$BASEINPUT/structured/torus5.gr"


mkdir -p "$REPORTS_DIR"
cd "amq_reports"
# Generate results
cat $RESULTDIR/*.log | python "$BASE_SCRIPTS/report.py" > report.csv
Rscript "$BASE_SCRIPTS/report.R" report.csv report.json

if (($SKIPPED)); then
  echo -en '\033[1;32m'
  echo -n "SOME OK (skipped $SKIPPED)"
  echo -e '\033[0m'
else
  echo -en '\033[1;32m'
  echo -n ALL OK
  echo -e '\033[0m'
fi
