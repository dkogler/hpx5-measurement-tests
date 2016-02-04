#!/usr/bin/env bash

function init() {
    if [ $1 -ne 2 ]; then
	echo "Usage: $2 <contrib> <tarball>"
	exit 1
    fi
    
    for c in rsync md5sum; do
	command -v $c >/dev/null 2>&1 ||
	{ echo "This script requires $c but it's not installed.  Aborting." >&2; 
	    exit 1; }
	eval $c=`command -v $c`
    done
}

init $# $0

CONTRIB=$1
TARBALL=$2

USER=${USER}
DEST="stout.crest.iu.edu:/var/www/hpx"

if [ ! -f $TARBALL ]; then
    echo "Could not find tarball file: $TARBALL"
    exit 1
fi

md5=($($md5sum $TARBALL))
echo "Uploading $CONTRIB tarball with md5 ${md5}"
$rsync -arv $TARBALL ${USER}@${DEST}/${CONTRIB}/${md5}/

