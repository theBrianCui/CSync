#!/usr/bin/env bash
drift="$1"
rapport="$2"
amort="$3"
timeout="$4"
dir="$5"

mkdir -p "results/${dir}/${drift}"
output="results/${dir}/${drift}/${drift}_${rapport}_${amort}.txt"
echo "Generating ${output}"
./client 128.83.139.94 "80${drift}" "$drift" "-${drift}" -3.5 1800 "$rapport" "${timeout}" "$amort" 50000 > "$output"
