# CoAChecker

## Overview

This project is an implementation of the paper "CoAChecker: Safety Analysis on Administrative Policies of Cyber-Oriented Access Control".

CoAChecker is a tool designed to analyze the safety of ACoAC policies. It uses formal methods to check whether ACoAC policies meet the intended safety requirements.

## Installation

1. Clone the repository
2. Make sure you have the required dependencies:
   - gcc
   - cmake
3. Download [nuXmv](https://nuxmv.fbk.eu/download.html)
   ```bash
   cd coachecker
   wget -O nuXmv-2.1.0-linux64.tar.xz https://nuxmv.fbk.eu/theme/download.php?file=nuXmv-2.1.0-linux64.tar.xz
   tar Jxvf nuXmv-2.1.0-linux64.tar.xz
   mv nuXmv-2.1.0-linux64/ nuXmv
   ``` 
4. Build and compile the project, the executable file will be in the bin directory
   ```bash
   cd bin
   ./build.sh clean
   ./build.sh
   ```

## Usage

1. **To analyze an ACoAC policy file:**

   ```bash
   ./coachecker -model_checker <nuXmv_path> -input <policy_file> -timeout <timeout_threshold_in_seconds> -l <log_dir>
   ```

   Example:
   ```bash
   ./coachecker -model_checker ../nuXmv/bin/nuXmv -input ../demo/demo1.acoac -timeout 60 -l ../logs
   ```

   For more options, please see the help message:
   ```bash
   ./coachecker -h
   ```

2. **To evaluate the impact of instance scale on the performance of pruning, abstraction refinement, and bound estimation:**
   - Download the [dataset](https://drive.google.com/uc?id=1c2-EPhiTKJVPyTu7EH6PfKtd427JaouG&export=download)

      ```bash
      cd coachecker
      mkdir data && cd data
      ```
   
   - Evaluate the performance of pruning

      Run the following script and wait for it to complete

      ```bash
      cd coachecker/bin
      mkdir -p ../logs
      nohup ./eval_pruning.sh > ../logs/exp_pruning.log 2>&1 &
      ```

      The results are saved in the tail of the file "<coachecker_home>/logs/exp_pruning.log" when the script completes.

   - Evaluate the performance of abstraction refinement

      Run the following script and wait for it to complete

      ```bash
      cd coachecker/bin
      nohup ./eval_absref.sh -n ../nuXmv -l ../logs/eval_absref -o ../data/ARR.csv > ../logs/eval_absref_progress 2>&1 &
      ```

      Check the running progress

      ```bash
      tail -f ../logs/eval_absref_progress
      ```

      The results are saved in the file "<coachecker_home>/data/ARR.csv" when the script completes.

   - Evaluate the performance of bound estimation

      ```bash
      cd coachecker/bin
      ./eval_bound.sh ./bound_tightness.csv
      ```

3. **To determine the individual contribution of each component (*pruning*, *abstraction refinement*, and *bound estimation*):**
   
   - Download the [dataset](https://drive.google.com/uc?id=1c2-EPhiTKJVPyTu7EH6PfKtd427JaouG&export=download)

      ```bash
      cd coachecker/data
      wget xxx
      ```

   - Run the script "bin/eval_efficiency.sh" and wait for it to complete

      ```bash
      cd coachecker/bin
      mkdir -p ../logs
      nohup ./eval_efficiency.sh -a -n ../nuXmv -t 600 -l ../logs/ablation > ../logs/ablation_progress 2>&1 &
      ```

      Check the running progress

      ```bash
      tail -f ../logs/ablation_progress
      ```
   
   - After the script completes, analyze the output logs of coachecker in the folder "logs/ablation"

      ```bash
      ./compare.sh ../logs/ablation ./ablation.csv
      ```

      **Note.** The script `eval_efficiency.sh` may take a long time to complete because there are **15000** instances in the datasets. For each instance, the script will run coachecker for **4** times with different configurations:  
      - all components are enabled
      - pruning is disabled
      - abstraction refinement is disabled
      - bound estimation is disabled
      
      For each run, the script sets the timeout threshold to **600** seconds. According to the test on a XX server, it requires about **72** hours to complete the experiments. To reduce the running time, it is recommended to set the timeout threshold to a smaller value.

4. **To compare the performance of coachecker with VAC and Mohawk:**

   - Download the [dataset](https://drive.google.com/uc?id=1c2-EPhiTKJVPyTu7EH6PfKtd427JaouG&export=download)

      ```bash
      cd coachecker/data
      wget xxx
      ```
   
   - Install VAC and Mohawk

   - Run the script "bin/eval_efficiency.sh" and wait for it to complete

      ```bash
      cd coachecker/bin
      mkdir -p ../logs
      nohup ./eval_efficiency.sh -c -m -v <vac_home> -n ../nuXmv -t 600 -l ../logs/comp-coachecker-mohawk-vac > ../logs/comp_cmv_progress 2>&1 &
      ```

      Check the running progress

      ```bash
      tail -f ../logs/comp_cmv_progress
      ```

   - After the script completes, analyze the output logs of coachecker in the folder "logs/comp-coachecker-mohawk-vac"

      ```bash
      ./log_analyzer -l ../logs/comp-coachecker-mohawk-vac/<dataset_name>
      ```
