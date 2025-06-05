#!/bin/bash

usage() {
  echo "usage: ./evaluate_absref.sh "
  echo "  -l <log_dir>      the directory to save the evaluation log files, MUST be provided"
  echo "  -t <timeout>      the timeout threshold (default: 600)"
  echo "  -n <nuXmv_home>   the home directory of nuXmv, MUST be provided"
  echo "  -o <result_file>  the file to save the evaluation results, MUST be provided"
  echo "  -h                show this help message and exit"
  exit 1
}

while getopts "l:t:n:o:h" opt; do
  case $opt in
    l)
      log_dir=$OPTARG
      ;;
    t)
      timeout=$OPTARG
      ;;
    n)
      nuXmv_home=$OPTARG
      ;;
    o)
      result_file=$OPTARG
      ;;
    h)
      usage
      exit 0
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      usage
  esac
done

if [ ! -f ./coachecker ]; then
    echo "coachecker executable is not found"
    echo "please compile coachecker first, see README.md for more details"
    exit 1
fi
if [ -z ${nuXmv_home} ]; then
  # nuXmv directory is not provided, exit
  echo "nuXmv home directory is not provided"
  usage
  exit 1
fi
if [ ! -d ${nuXmv_home} ]; then
  # nuXmv directory does not exist, exit
  echo "nuXmv home directory ${nuXmv_home} does not exist"
  exit 1
fi
if [ ! -f ${nuXmv_home}/bin/nuXmv ]; then
  echo "nuXmv executable is not found in ${nuXmv_home}/bin"
  exit 1
fi

# if the timeout is not provided, set it to 600
if [ -z ${timeout} ]; then
  echo "timeout threshold is set to 600 seconds by default"
  timeout=600
else
  echo "timeout threshold is set to ${timeout} seconds"
fi

dataset_dir=../data/ACoAC_Datasets

# check whether the dataset directory exists
if [ ! -d ${dataset_dir} ]; then
  echo "dataset directory ${dataset_dir} does not exist"
  exit
fi

# the number of instances in a dataset
num=50

# the file to save the ARR results
if [ -z ${result_file} ]; then
  echo "result file is not provided"
  usage
  exit 1
elif [ -f ${result_file} ]; then
  echo "result file ${result_file} already exists"
  exit 1
fi

rm -rf ${result_file}

if [ -z ${log_dir} ]; then
  # log directory is not provided, exit
  echo "log directory is not provided"
  usage
  exit 1
elif [ ! -d ${log_dir} ]; then
  # log directory does not exist, create the log directory
  mkdir -p ${log_dir}
  echo "the log files will be saved in ${log_dir}"
else
  echo "the log files will be saved in ${log_dir}"
fi

# for figure (a)
for a in `seq 4`; do
  attrNum=`expr ${a} + 1`
  attrNum=`expr ${attrNum} \* 3`
  for k in `seq 10`; do
    ruleNum=`expr ${k} \* 50`
    subdir=MC5-Q1-V10-A${attrNum}/P${ruleNum}

    if [ -z ${dataset_dir}/$subdir ]; then
      echo "dir ${dataset_dir}/$subdir is empty"
      exit
    fi

    rm -rf ${log_dir}/${subdir}
    mkdir -p ${log_dir}/${subdir}

    for id in `seq ${num}`; do
      file=${dataset_dir}/${subdir}/test${id}.acoac
      echo "analyzing $file"
      (time ./coachecker -p -t 600 -m ${nuXmv_home}/bin/nuXmv -i ${file} -l ${log_dir}/) >& ${log_dir}/${subdir}/output${id}-all.txt
    done

    ./log_analyzer -l ${log_dir}/${subdir} -o ${resultFile} -a
  done
done

# for figure (b)
for mc in `seq 4`; do
  maxCond=`expr ${mc} + 2`
  for k in `seq 10`; do
    domSize=`expr ${k} \* 10`
    subdir=MC${maxCond}-Q1-A9-R300/V${domSize}
    
    if [ -z ${dataset_dir}/$subdir ]; then
      echo "dir ${dataset_dir}/$subdir is empty"
      exit
    fi

    rm -rf ${log_dir}/${subdir}
    mkdir -p ${log_dir}/${subdir}

    for id in `seq ${num}`; do
      file=${dataset_dir}/${subdir}/test${id}.acoac
      echo "analyzing $file"
      (time ./coachecker -p -t 600 -m ${nuXmv_home}/bin/nuXmv -i ${file} -l ${log_dir}/) >& ${log_dir}/${subdir}/output${id}-all.txt
    done

    ./log_analyzer -l ${log_dir}/${subdir} -o ${resultFile} -a
  done
done

# for figure (c)
for Aq in `seq 5`; do
  for k in `seq 10`; do
    domSize=`expr ${k} \* 10`
    subdir=MC5-Q${Aq}-A30-R1500/V${domSize}

    if [ -z ${dataset_dir}/$subdir ]; then
      echo "dir ${dataset_dir}/$subdir is empty"
      exit
    fi

    rm -rf ${log_dir}/${subdir}
    mkdir -p ${log_dir}/${subdir}

    for id in `seq ${num}`; do
      file=${dataset_dir}/${subdir}/test${id}.acoac
      echo "analyzing $file"
      (time ./coachecker -p -t 600 -m ${nuXmv_home}/bin/nuXmv -i ${file} -l ${log_dir}/) >& ${log_dir}/${subdir}/output${id}-all.txt
    done

    ./log_analyzer -l ${log_dir}/${subdir} -o ${resultFile} -a
  done
done

echo "Done"
echo "The results are saved in ${result_file}"