#!/usr/bin/env python
from matplotlib.pyplot import *
from matplotlib.font_manager import FontProperties
import numpy as np
import matplotlib.mlab as mlab
import matplotlib.ticker as ticker
import sys, random, re

fig = figure(figsize=(16, 9))
data = []

j = 0
for i, fn in enumerate(sys.argv[1:]):
#    num = re.search(r'\d+', fn).group()
    with file(fn) as f:
        for l in f:
            w = l.split()
            data.append(1000*float(w[4]))
            j = j+1

# the histogram
print j

vals = np.asarray(data)
results, edges = np.histogram(vals, bins=70, range=(.4, 1), normed=True)
binWidth = edges[1] - edges[0]
bar(edges[:-1], results*binWidth*100, binWidth, color='green')

# titles, font, etc.
ylabel('Percentage', size='x-large')
xmin = .4
xmax = 1
axis([xmin, xmax, 0, 5])
xticks(np.arange(xmin, xmax, .05))
# yticks(np.arange(n)+0.25, corenum)
#yscale('log', nonposy='clip')
xlabel('Time [us]', size='x-large')
# tick_params(axis='y', which='major', labelsize=12)
title('Histogram of context switch timings', size='xx-large')
font = {'family' : 'normal',
        'weight' : 'bold',
        'size'   : 22}
rc('font', **font)
show()
