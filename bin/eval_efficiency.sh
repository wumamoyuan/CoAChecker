#!/bin/bash

usage() {
  echo "usage: ./evaluate_efficiency.sh "
  echo "  -a                running CoAChecker and three variants (no_slicing, no_absref, smc)"
  echo "  -c                running CoAChecker"
  echo "  -m                runing Mohawk"
  echo "  -v <vac_home>     runing VAC"
  echo "  -l <log_dir>      the directory to save the evaluation log files, MUST be provided"
  echo "  -t <timeout>      the timeout threshold (default: 600)"
  echo "  -n <nuXmv_home>   the home directory of nuXmv, MUST be provided"
  echo "  -h                show this help message and exit"
  exit 1
}

ablation=0
coachecker=0
mohawk=0

while getopts "acmv:l:t:n:h" opt; do
  case $opt in
    a)
      echo "running CoAChecker and three variants (no_slicing, no_absref, smc)"
      ablation=1
      ;;
    c)
      echo "running CoAChecker"
      coachecker=1
      ;;
    m)
      echo "running Mohawk"
      mohawk=1
      ;;
    v)
      echo "running VAC"
      vac_home=$OPTARG
      ;;
    l)
      log_dir=$OPTARG
      ;;
    t)
      timeout=$OPTARG
      ;;
    n)
      nuXmv_home=$OPTARG
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

if [ ${ablation} -eq 0 ] && [ ${coachecker} -eq 0 ] && [ ${mohawk} -eq 0 ] && [ -z ${vac_home} ]; then
  echo "no tool is selected"
  usage
  exit 1
fi

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

# if the timeout is not provided, set it to 600
if [ -z ${timeout} ]; then
  echo "timeout threshold is set to 600 seconds by default"
  timeout=600
else
  echo "timeout threshold is set to ${timeout} seconds"
fi

# if CoAChecker or the variants are selected, check whether the CoAChecker executable exists
if [ ${coachecker} -eq 1 ] || [ ${ablation} -eq 1 ]; then
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
fi

# if Mohawk is selected, check whether the Mohawk home directory exists
if [ ${mohawk} -eq 1 ]; then
  if [ ! -f ../lib/mohawk-idea-1.0.0.jar ] || [ ! -f ../lib/antlr-4.4-complete.jar ]; then
    echo "Mohawk jar files are not found in ../lib"
    echo "please compile Mohawk and copy the jar files to ../lib first, see README.md for more details"
    exit 1
  fi
  for f in ../lib/*.jar; do
    CLASSPATH=${CLASSPATH}:${f}
  done
fi

# if VAC is selected, check whether the VAC home directory exists
if [ ! -z ${vac_home} ]; then
  if [ ! -d ${vac_home} ]; then
    echo "VAC home directory ${vac_home} does not exist"
    exit 1
  elif [ ! -f ${vac_home}/vac_static/vac.sh ]; then
    echo "VAC running script is not found in ${vac_home}/vac_static"
    echo "please check whether the VAC is installed correctly"
    exit 1
  fi
fi

dataset_dir=../data/URA_Datasets

# check whether the dataset directory exists
if [ ! -d ${dataset_dir} ]; then
  echo "dataset directory ${dataset_dir} does not exist"
  exit
fi

# record the current directory
current_dir=$(pwd)
echo "current directory: ${current_dir}"

for i in `seq 3`; do
  min_cond=$(($i - 1))
  for k in `seq 4`; do
    s=$((1 + $k))
    ruleNum=$((5000 * $s))
    subdir=D-${min_cond}-$ruleNum

    if [ -z ${dataset_dir}/$subdir ]; then
      echo "dir ${dataset_dir}/$subdir is empty"
      exit
    fi

    rm -rf ${log_dir}/${subdir}
    mkdir -p ${log_dir}/${subdir}

    for id in `seq 50`; do
      file=${current_dir}/${dataset_dir}/$subdir/test${id}.mohawk
      echo "analyzing $file"
      if [ ${coachecker} -eq 1 ] || [ ${ablation} -eq 1 ]; then
        (time ./coachecker -t $timeout -m ${nuXmv_home}/bin/nuXmv -i $file -l ${log_dir}) >& ${log_dir}/${subdir}/output${id}-all.txt
      fi
      if [ ${ablation} -eq 1 ]; then
        (time ./coachecker -t $timeout -m ${nuXmv_home}/bin/nuXmv -i $file -l ${log_dir} -no_slicing) >& ${log_dir}/${subdir}/output${id}-noslicing.txt
        (time ./coachecker -t $timeout -m ${nuXmv_home}/bin/nuXmv -i $file -l ${log_dir} -no_absref) >& ${log_dir}/${subdir}/output${id}-noabsref.txt
        (time ./coachecker -t $timeout -m ${nuXmv_home}/bin/nuXmv -i $file -l ${log_dir} -smc) >& ${log_dir}/${subdir}/output${id}-smc.txt
      fi
      if [ ${mohawk} -eq 1 ]; then
        (time java -cp ${CLASSPATH} mohawk.Mohawk -timeout $timeout -nusmv ${nuXmv_home}/bin/nuXmv -run all -mode bmc -input $file) >& ${log_dir}/${subdir}/output${id}-mohawk.txt
      fi
      if [ ! -z ${vac_home} ]; then
        cd ${vac_home}/vac_static
        (time ./vac.sh $file) >& ${current_dir}/${log_dir}/${subdir}/output${id}-vac.txt
        cd ${current_dir}
      fi
    done
  done
done

subdir=D-real

if [ -z ${dataset_dir}/$subdir ]; then
  echo "dir ${dataset_dir}/$subdir is empty"
  exit
fi

rm -rf ${log_dir}/${subdir}
mkdir -p ${log_dir}/${subdir}

for id in `seq 50`; do
  file=${current_dir}/${dataset_dir}/$subdir/test${id}.mohawk
  echo "analyzing $file"
  if [ ${coachecker} -eq 1 ] || [ ${ablation} -eq 1 ]; then
    (time ./coachecker -t $timeout -m ${nuXmv_home}/bin/nuXmv -i $file -l ${log_dir}) >& ${log_dir}/${subdir}/output${id}-all.txt
  fi
  if [ ${ablation} -eq 1 ]; then
    (time ./coachecker -t $timeout -m ${nuXmv_home}/bin/nuXmv -i $file -l ${log_dir} -no_slicing) >& ${log_dir}/${subdir}/output${id}-noslicing.txt
    (time ./coachecker -t $timeout -m ${nuXmv_home}/bin/nuXmv -i $file -l ${log_dir} -no_absref) >& ${log_dir}/${subdir}/output${id}-noabsref.txt
    (time ./coachecker -t $timeout -m ${nuXmv_home}/bin/nuXmv -i $file -l ${log_dir} -smc) >& ${log_dir}/${subdir}/output${id}-smc.txt
  fi
  if [ ${mohawk} -eq 1 ]; then
    (time java -cp ${CLASSPATH} mohawk.Mohawk -timeout $timeout -nusmv ${nuXmv_home}/bin/nuXmv -run all -mode bmc -input $file) >& ${log_dir}/${subdir}/output${id}-mohawk.txt
  fi
  if [ ! -z ${vac_home} ]; then
    cd ${vac_home}/vac_static
    (time ./vac.sh $file) >& ${current_dir}/${log_dir}/${subdir}/output${id}-vac.txt
    cd ${current_dir}
  fi
done

echo "done"