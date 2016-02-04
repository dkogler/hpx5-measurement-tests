#!/bin/bash
#
#SBATCH -N 1
#SBATCH -n 1
#SBATCH -t 0-2:00
#SBATCH -o sturm.%N.%j.out

mpirun -np 1 ./my_thread_create --hpx-threads=1 --hpx-inst-dir=$HOME/hpxmeasurements/tc --hpx-prof-detailed
#optionally, if building with papi add --hpx-prof-counters=...

