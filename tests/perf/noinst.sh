#!/bin/bash
#
#SBATCH -N 1
#SBATCH -n 1
#SBATCH -t 0-2:00
#SBATCH -o sturm.%N.%j.out

./thread_switch

