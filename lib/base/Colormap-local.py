import numpy as np
import sys
from minieigen import *
pm=.1
sys.stdout.write('\t\tColormap{"red-green-blue",{')
for a in np.linspace(0,1,255):
    m=3*(a%(1/3.)) # relative within that third
    i=int(a/(1/3.))
    if i==3: i,m=2,1
    c=[Vector3(.87,0,0),Vector3(0,.82,0),Vector3(.36,.58,1.)][i] # something isoluminant, by eye
    c[i]-=(1-m)*pm
    if a>0.: sys.stdout.write(',')
    sys.stdout.write('%g,%g,%g'%(c[0],c[1],c[2]))
    #print i,c[0],c[1],c[2]
sys.stdout.write('}}\n') # add comma if there are other colormaps

