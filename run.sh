#!/usr/bin/env zsh

if [ -z "$1" ]; then
  echo "Usage: $0 <input_file>"
  exit 1
fi

server_name=$(hostname)
input_file=$1

./build/apps/optimal --platform platforms/exynos5422.json --alloc opti --sched grub --output logs.json --input "$input_file"
# curl -d "server ${server_name} finished ${input_file}" ntfy.sh/fillien-hpc
