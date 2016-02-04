#!/usr/bin/python

# This script adds a standard copyright header to the files
# specified on the command-line. If no file is specified, it
# operates on all files in the current directory.
#
# It also correctly takes into account the copyright year, and
# adds a range, if necessary.
#
# NOTE: this script updates the files in-place, so use it carefully!
#

import sys
import os
import re
from datetime import date

# The default institution (Set this to your institution)
defaultinst = "Trustees of Indiana University"

copyfmt = "// Copyright (c) %s, %s"
header = """// All rights reserved. This software may be modified
// and distributed under the terms of the BSD license.
// See the COPYING file for details.

//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).

"""

currentyear = date.today().year
defaultcopyfmt = copyfmt % (str(currentyear), defaultinst)

# The input year is a RE match object
# Only update the year if the institution is the default institution
def fix_copyright(c):
    startyear = int(c[0])
    endyear = int(c[1]) if c[1] else 0
    inst = c[2].strip()

    if inst == defaultinst:
        if startyear < currentyear:
            return copyfmt % (str(startyear)+"-"+str(currentyear), inst)
        else:
            return defaultcopyfmt
    else:
        if endyear:
            return copyfmt % (str(startyear)+"-"+str(endyear), inst)
        else:
            return copyfmt % (str(startyear), inst)

# Given a list of match objects, generate the new copyright header from
# the above template
def new_header(cs):
    c = "\n".join(map(fix_copyright, cs)) if cs else defaultcopyfmt
    return c+"\n"+header

def main():
    comment = re.compile("^//.*$|^/\*.*\*/$", re.S)
    close = re.compile(".*\*/$")
    yre = re.compile("^.*(?<=Copyright \(c\) )(\d{4})-?(\d{2}|\d{4})?, (.*)\n", re.M)

    # Get the list of files to add the
    if len(sys.argv) > 1:
        files = sys.argv[1:]
    else:
        files = [ f for f in os.listdir(os.getcwd()) if os.path.isfile(f) and (f.endswith(".c") or f.endswith(".h")) ]
  
    # Check the copyright header
    for f in files:
        with open(f, "r+") as handle:
            lines = handle.readlines()
            started = 0
            hdr = ""
            body = ""
            for line in lines:
                l = line.strip()
                # gobble up initial empty lines
                if started == 0 and l == "":
                    continue
                # look for single-line comments
                elif started >= 0 and comment.match(l):
                    started = 2
                    hdr += line
                    continue
                # begin multiline comment
                elif started == 0 and l[:2] == '/*':
                    started = 1
                    hdr += line
                    continue
                # close multiline comment
                elif started == 1:
                    if close.match(l):
                        started = -1
                    hdr += line
                    continue
                # Blank lines are delimiters for single-line comment headers
                elif started == 2 and l == "":
                    started = -1
                    hdr += line
                    continue
                else:
                    started = -1
                    body += line

            newhdr = new_header(re.findall(yre, hdr))
            if hdr != newhdr:
                handle.seek(0)
                handle.writelines(newhdr)
                handle.writelines(body)
                handle.truncate()
                print "Fixed header for file: ", f
            else:
                print "Skipping file: ", f

if __name__ == "__main__":
    sys.exit(main())
