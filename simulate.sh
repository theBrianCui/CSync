#!/usr/bin/env bash
drift="$1"
rapport="$2"
amort="$3"

mkdir -p "results/$drift"
output="results/${drift}/${drift}_${rapport}_${amort}.txt"
echo "Generating ${output}"
./client 128.83.139.94 8080 "$drift" "-${drift}" -16 1800 "$rapport" 4000000 "$amort" 50000 > "$output"
