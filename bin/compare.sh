#!/bin/bash

if [ $# -ne 2 ]; then
  echo "Usage: $0 <log_directory> <output_csv_file>"
  exit 1
fi

log_directory=$1
output_csv_file=$2

if [ -f ${output_csv_file} ]; then
  echo "${output_csv_file} already exists"
  echo "Do you want to overwrite it? (y/n)"
  read overwrite
  if [ ${overwrite} != "y" ]; then
    exit 1
  fi
  rm -rf ${output_csv_file}
fi

for i in `seq 3`; do
  min_cond=$(($i - 1))
  for k in `seq 4`; do
    s=$((1 + $k))
    ruleNum=$((5000 * $s))
    subdir=MinC${min_cond}-P${ruleNum}

    ./log_analyzer -l ${log_directory}/${subdir} -o ${output_csv_file}
  done
done

subdir=D-real

./log_analyzer -l ${log_directory}/${subdir} -o ${output_csv_file}

echo "Done"
echo "The results are saved in ${output_csv_file}"