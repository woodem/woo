import woo.dem, numpy, matplotlib.pyplot
fig=matplotlib.pyplot.figure()
ax=fig.add_subplot(111)
sigN=numpy.linspace(-1.5e7,.5e7,100)
cc=woo.dem.Law2_L6Geom_ConcretePhys()
for t in 'linear','log+lin':
   for omega in (0,.5,1):
      cc.yieldSurfType=t
      sigT=[cc.yieldSigmaTNorm(sigmaN=s,omega=omega,coh0=3.5e6,tanPhi=.8) for s in sigN]
      ax.plot(list(sigN)+[float('nan')]+list(sigN),sigT+[float('nan')]+[-s for s in sigT],label=cc.yieldSurfType+", $\omega$=%g"%omega,lw=3,alpha=.5)
ax.set_xlabel(r'$\sigma_N$'); ax.set_ylabel(r'$\max\,\sigma_T$')
ax.legend(framealpha=.5)
ax.grid(True)
fig.show()