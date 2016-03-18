#!/usr/bin/env python

### This is the script for new stack frame filter that removes HPX frames
### from the output of 'backtrace' command.  

import gdb
from collections import Iterator

class HPXFrameFilter():
  
  def __init__(self):
    # Frame filter attribute creation.
    # 'name' is the name of the filter that GDB will display.
    # 'priority' is the priority of the filter relative to other filters.
    # 'enabled' is a boolean that indicates whether this filter is enabled 
    # and should be executed.
    self.name = "Hide_HPX_Frames"
    self.priority = 100
    self.enabled = True
    # Register this frame filter with the global frame_filters dictionary.
    gdb.frame_filters[self.name] = self
    
  def filter(self, frame_iterator):
    # Return a new iterator that does not have HPX stack frames.
    return HPXFrameIterator(frame_iterator)

class HPXFrameIterator(Iterator):

  def __init__(self, input_iterator):
    self.input_iterator = input_iterator
  
  def __iter__(self):
    return self
  
  def __next__(self):
    # Loop until non-HPX frame is found and return it. 
    # If end is reached return None.
    while True:
      frame_decorator = next(self.input_iterator)
      # Find the absolute file name for the current function.
      symtab_and_line = frame_decorator.inferior_frame().find_sal()
      absolute_file_name = symtab_and_line.symtab.fullname()
      # If the absolute file name does not contain '/hpx/' it's not 
      # an HPX file return the frame.
      if absolute_file_name.find("/hpx/") == -1:
        return frame_decorator

HPXFrameFilter()

