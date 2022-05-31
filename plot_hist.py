import matplotlib.pyplot as plt
import numpy as np

with open('bw.txt') as f:
    x = [float(line) for line in f.readlines()]
maxV = max(x)
minV = 0
binsN = int(maxV/10) + 1
plt.xlabel('BW (Mbps)')
plt.ylabel('Count')
plt.xlim(0,100)
plt.hist(x, bins=binsN)
plt.show() 
