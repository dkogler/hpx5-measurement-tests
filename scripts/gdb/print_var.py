#!/usr/bin/env python

### This script is for new gdb command "hpx print"

import gdb

def validate_args(list_of_args):
  '''Validate the arguments passed to the current command.'''
  
  if len(list_of_args) != 2:
    return 1
    
  addr = int(list_of_args[0])
  if addr < 0:
    return 2
  
  temp_sym = gdb.lookup_symbol(list_of_args[1])
  if temp_sym[0] == None:
    return 3
  
  return 0 

def print_variable(var_hpx_addr, cast_to):
  '''Access Global Address Space(GAS) to retrieve the variable at given ''' \
      '''hpx address.'''
  
  temp_str = "set $req_val = (" + cast_to + " *) malloc(sizeof(" + cast_to \
      + "))"
  gdb.execute(temp_str)
  temp_str = "sizeof(" + cast_to + ")"
  length = gdb.parse_and_eval(temp_str)
  temp_str = "hpx_gas_memget_sync($req_val, " + var_hpx_addr + ", " + \
      str(length) + ")"
  rs = gdb.parse_and_eval(temp_str)
  temp_str = "print *$req_val"
  gdb.execute(temp_str)
  return 0

class hpx_print(gdb.Command):
  """This command prints variable referenced by hpx address. 
Syntax:
      hpx print <hpx address> <data type to be used for casting the result>"""
  def __init__(self):
    gdb.Command.__init__(self, "hpx print", gdb.COMMAND_DATA, \
        gdb.COMPLETE_SYMBOL)
  
  def invoke(self, args, from_tty):
    list_of_args = gdb.string_to_argv(args)
    
    rc = validate_args(list_of_args)
    if rc == 1: 
      print ('Invalid arguments. Try "help hpx print".')
    elif rc == 2:
      print ('Invalid HPX Address.')
    elif rc == 3:
      print ('Invalid casting data type.')
    elif rc == 0:
      var = list_of_args[0]
      cast_to = list_of_args[1]
      print_variable(var, cast_to)

hpx_print()

