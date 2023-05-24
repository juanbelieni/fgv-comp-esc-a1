#!/bin/bash

n=$1

run_python_script() {
  python main.py -l 6 -pl 0.1 -pv 0.1 -pc 0.05 -vmin 1 -d 0.1
}

for ((i=1; i<=n; i++))
do
  echo "Iniciando $i" &
  run_python_script $i &
done

wait
