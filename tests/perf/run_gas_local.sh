#!/bin/bash
#
#SBATCH -N 2
#SBATCH -n 16
#SBATCH -t 0-2:00
#SBATCH -o sturm.%N.%j.out

#Warning: This one may crash several times before running properly, I don't know if the problem is the instrumentation or something else

mpirun --map-by node:PE=1 -n 1 ./my_gas_local --hpx-inst-dir=$HOME/hpxmeasurements/gl --hpx-prof-detailed
#optionally, if building with papi add --hpx-prof-counters=...

