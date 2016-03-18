#!/usr/bin/env python

### This script is for new gdb prefix command "hpx" 

import gdb

class HPXPrefixCommand (gdb.Command):
  "Prefix command for HPX commands."

  def __init__ (self):
    super (HPXPrefixCommand, self).__init__ ("hpx", gdb.COMMAND_SUPPORT, 
        gdb.COMPLETE_NONE, True)

HPXPrefixCommand()

