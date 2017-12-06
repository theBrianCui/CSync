#!/usr/bin/env bash
drift="$1"
rapport="$2"
amort="$3"

mkdir -p "results/monolith/$drift"
output="results/monolith/${drift}/${drift}_${rapport}_${amort}.txt"
echo "Generating ${output}"
./client 128.83.139.94 "80${drift}" "$drift" "-${drift}" -3.5 1800 "$rapport" 400000 "$amort" 50000 > "$output"
