import matplotlib
matplotlib.rc('text',usetex=True)
matplotlib.rc('text.latex',preamble=r'\usepackage{concrete}\usepackage{euler}')

import pylab
# from yade import eudoxos
from woo.dem import *

sigMinMax=-30e6,5e6
sig=pylab.arange(sigMinMax[0],sigMinMax[1],(sigMinMax[1]-sigMinMax[0])/1000)
sigg=pylab.concatenate((sig,sig[::-1]))
nFig=2
law=Law2_L6Geom_ConcretePhys(yieldLogSpeed=.4,yieldEllipseShift=-10e6)
# plastConditions=['linear','parabolic','log','log+lin','elliptic','elliptic+lin']
for plastCondition in ['loglin']: #,'log+lin']:
	#pylab.figure()
	for n,omega in enumerate(pylab.arange(0,1+(1./nFig),1./nFig)):
		pylab.axvline(linewidth=2,color='k')
		law.yieldSurfType=plastCondition
		pylab.plot(sigg,[(1 if i<len(sigg)/2 else -1)*law.yieldSigmaTNorm(sigmaN=s,omega=omega,coh0=3.5e6,tanPhi=.8) for i,s in enumerate(sigg)],label='$\hbox{%s,}\,\omega$=%g'%(plastCondition,omega),linewidth=2,alpha=.6)
		pylab.xlabel('$\sigma_N$'); pylab.ylabel(r'$\pm|\sigma_T|$')
	#pylab.show()
pylab.legend()
pylab.grid(True)
#pylab.gcf().set_size_inches(10,10)
# pylab.savefig('yield-surfaces.woo.pdf')
pylab.show()
quit()
