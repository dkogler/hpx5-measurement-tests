#!/usr/bin/env python

### This script is for new gdb command "info parcels"

import gdb

def print_parcels():
  ### Retrieve Action Table entries
  number_of_actions = gdb.parse_and_eval('here->actions->n')
  actions = gdb.parse_and_eval('here->actions->entries')

  ### Display parcels for current rank.
  print ("--------------------\nScheduler Parcels:\n--------------------")
  ### Retrieve Worker Threads
  number_of_workers = gdb.parse_and_eval('here->sched->n_workers')
  print ("Total number of worker threads: %d" % number_of_workers)
  workers = gdb.parse_and_eval('here->sched->workers')
  
  ### Loop through each worker thread and print all the parcels
  for i in range(0, int(number_of_workers)):
    print ("--------------------\nWorker %d info:\n--------------------" % i)
    worker = workers[i]
  
    ### Retrieve current parcel
    current = worker['current']
    print ("Current parcel address: %s" % current)
    current_parcel = current.dereference()
    action_nbr = current_parcel['action']
    print ("Current action: %s " % actions[action_nbr]['key']) 
    size = current_parcel['size']
    print ("Parcel size: %d" % size)
    data_ptr = current_parcel['buffer']
    print ("Buffer address: %s" % data_ptr)
  
class hpx_parcels(gdb.Command):
  '''This command prints all the parcels that are currently in execution.'''
  def __init__(self):
    gdb.Command.__init__(self, "info parcels", gdb.COMMAND_DATA)
  
  def invoke(self, arg, from_tty):
    print_parcels()
 
hpx_parcels()
