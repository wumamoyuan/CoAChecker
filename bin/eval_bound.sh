#!/bin/bash

dataset_dir=../data/ACoAC_Datasets
output=$1

for i in `seq 4`; do
  attrNum=`expr ${i} + 1`
  attrNum=`expr ${attrNum} \* 30`
    for j in `seq 10`; do
        ruleNum=$((500 * $j))
        subdir=MC5-Q1-V20-A${attrNum}/P${ruleNum}
        ./coachecker -c -i ${dataset_dir}/${subdir} -o ${output}
    done
done

for i in `seq 4`; do
    arRatio=0$(echo "0.03 * $i" | bc)
    for j in `seq 10`; do
        ruleNum=$((500 * $j))
        subdir=MC5-Q1-V20-AR${arRatio}/P${ruleNum}
        ./coachecker -c -i ${dataset_dir}/${subdir} -o ${output}
    done
done

echo "Done"
echo "The results are saved in ${output}"