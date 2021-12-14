from past.builtins import execfile
execfile('data-plot-energy.py')
S.run(2000,True)
S.plot.plot()
import pylab
pylab.gcf().set_size_inches(8,12)    # make higher so that it is readable
pylab.gcf().subplots_adjust(left=.1,right=.95,bottom=.05,top=.95) # make more compact