import numpy, pylab, woo.dem
r0,r1=0.7,1.0
rr=numpy.linspace(r0,r1,150)
expFunc=lambda r,gamma: ((r-r0)/(r1-r0))**gamma
kw=dict(linewidth=3,alpha=.5)
for gamma in (0,.2,1,2,10): pylab.plot(rr,[expFunc(r,gamma) for r in rr],label=r'$\gamma_t=%g$'%gamma,**kw)
l=pylab.legend(loc='best')
l.get_frame().set_alpha(.6)
pylab.ylim(-.05,1.05)
pylab.xlim(.9*r0,1.1*r1)
pylab.xlabel('radius')
pylab.ylabel(r'reduction of thinning rate $\theta_t$')
for r in (r0,r1): pylab.axvline(r,color='black',linewidth=2,alpha=.4)
pylab.grid(True)
pylab.suptitle('Reduction of $\\theta_t$ based on $r$ (with $r_0=1$, $r_{\min}^{\mathrm{rel}}=0.7$)')