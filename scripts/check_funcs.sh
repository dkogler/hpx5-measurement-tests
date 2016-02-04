#!/bin/bash

function is_file() {
    if [ ! -f $1 ]; then
	echo "File not found: $1"
	return 1
    fi
    return 0
}

function check_scan() {
    BOUT=$1
    is_file $BOUT || return
    if grep -q -E "[0-9]+ (bug|bugs) found" $BOUT; then
       DIR=`tail -n 1 $BOUT | egrep -o "[0-9]+-[0-9]+-[0-9]+-[0-9]+-[0-9]+-[0-9]"`;
       echo; echo "View report at http://`hostname`/scan-builds/$DIR";
       return 2;
    fi
}

function check_check() {
    BDIR=$1
    if [ -z $BDIR ]; then
	BDIR=`pwd`
    fi

    for TEST in "unit" "perf" "cxx"; do
	is_file $BDIR/tests/$TEST/test-suite.log || continue

	if grep '^# FAIL: *0$' $BDIR/tests/$TEST/test-suite.log
	then
	    echo "FAIL: 0"
	else
	    cat $BDIR/tests/$TEST/test-suite.log
	    return 2
	fi
	
	if egrep -q "(ERROR:)\s+[1-9][0-9]*" $BDIR/tests/$TEST/test-suite.log
	then
	    cat $BDIR/tests/$TEST/test-suite.log
	    return 2
	fi
    done
}

