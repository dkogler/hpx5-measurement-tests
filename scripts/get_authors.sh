#!/bin/sh

# Gets a list of unique authors that have contributed to the project.

git log --format='%aN <%aE>' | sort -u | sed 's/@/ [at] /g' | awk -F'<' '{ if (line[substr($2,0,7)]=="") line[substr($2,0,7)]=$0 } END { for (n in line) print line[n] }' | sort -u -k1,2
