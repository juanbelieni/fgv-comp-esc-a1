#!/bin/bash

n=$1

run_python_script() {
  python main.py -l 6 -pl 0.2 -pv 0.4 -pc 0.5 -vmin 1 -d 0.5
}

for ((i=1; i<=n; i++))
do
  echo "Iniciando $i" &
  run_python_script $i &
done

wait
