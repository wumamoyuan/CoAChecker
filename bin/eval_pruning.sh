#!/bin/bash

# the number of instances in a dataset
num=50
resultFile_a_r=../data/PruningRate-attrNum-ruleNum.csv
resultFile_mc_v=../data/PruningRate-maxCond-domSize.csv
resultFile_aq_v=../data/PruningRate-attrNumInQuery-domSize.csv

dataset_dir=../data/ACoAC_Datasets

# check whether the dataset directory exists
if [ ! -d ${dataset_dir} ]; then
  echo "dataset directory ${dataset_dir} does not exist"
  exit
fi

# for figure (a)
rm -rf ${resultFile_a_r}

for a in `seq 4`; do
  attrNum=`expr ${a} + 1`
  attrNum=`expr ${attrNum} \* 30`
  for k in `seq 10`; do
    ruleNum=`expr ${k} \* 500`
    subdir=MC5-Q1-V20-A${attrNum}/P${ruleNum}
    echo "evaluating on ${subdir}"

    for id in `seq ${num}`; do
      ./exp1 --instance-file ${dataset_dir}/${subdir}/test${id}.acoac --result-file ${resultFile_a_r}
    done
  done
done

# for figure (b)
rm -rf ${resultFile_mc_v}

for mc in `seq 4`; do
  maxCond=`expr ${mc} + 2`
  for k in `seq 10`; do
    domSize=`expr ${k} \* 10`
    subdir=MC${maxCond}-Q1-A90-R3000/V${domSize}
    echo "evaluating on ${subdir}"

    for id in `seq ${num}`; do
      ./exp1 --instance-file ${dataset_dir}/${subdir}/test${id}.acoac --result-file ${resultFile_mc_v}
    done
  done
done

# for figure (c)
rm -rf ${resultFile_aq_v}

for Aq in `seq 5`; do
  for k in `seq 10`; do
    domSize=`expr ${k} \* 10`
    subdir=MC5-Q${Aq}-A60-R3000/V${domSize}
    echo "evaluating on ${subdir}"

    for id in `seq ${num}`; do
      ./exp1 --instance-file ${dataset_dir}/${subdir}/test${id}.acoac --result-file ${resultFile_aq_v}
    done
  done
done

# compute the average GPR and LPR of each dataset
./exp1 --stat a-r --result-file ${resultFile_a_r}
echo ""
./exp1 --stat mc-v --result-file ${resultFile_mc_v}
echo ""
./exp1 --stat aq-v --result-file ${resultFile_aq_v}
echo ""