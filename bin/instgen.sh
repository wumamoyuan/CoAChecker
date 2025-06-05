#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Usage: $0 <store_dir> <instnum>"
    exit 1
fi

# The directory to store the generated instances
store_dir=$1
# The number of instances to generate
instnum=$2

# The number of users
usernum=2
# The number of boolean attributes
boolnum=3
# The number of integer attributes
intnum=3
# The number of string attributes
strnum=3
# The size of the domain of each attribute
domsize=10
# The number of rules
nrules=50
# The minimum number of atom conditions in a condition
minatomconds=1
# The maximum number of atom conditions in a condition
maxatomconds=5
# The number of attribute-value pairs in the safety query
nqueryav=2

# This parameter is used in the generation of initial state
# If it is p, then the initial value of an attribute is the default value 
# with probability p + (1 - p) * 1/domsize, and is other value with probability (1 - p) * 1/domsize
# E.g., suppose the domain size of an attribute is 10. If p = 0, then the initial value 
# is the default value with probability 0 + 1 * 1/10 = 0.1, and is other value with probability 1 * 1/10 = 0.1
# If p = 1, then the initial value must be the default value because p + (1 - p) * 1/domsize = 1
initavdefaultrate=0.2

if [ ! -d $store_dir ]; then
    mkdir -p $store_dir
    echo "the generated instances will be stored in $store_dir"
else
    echo "$store_dir already exists"
    exit 1
fi

for i in `seq $instnum`; do
    ./instgen -u $usernum -b $boolnum -s $strnum -n $intnum -d $domsize -i $initavdefaultrate -r $nrules -l $minatomconds -h $maxatomconds -q $nqueryav -o $store_dir/test$i.acoac
done