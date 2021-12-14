import numpy, pylab, woo.dem
pylab.figure(figsize=(8,6),dpi=160)
xx=numpy.linspace(0,-1e-3,100)
xxShort=xx[xx>-5e-4]
uNPl=-2e-4
alpha,kn,d0=3.,1e1,1e-3
ka=.1*kn
law=woo.dem.Law2_L6Geom_PelletPhys_Pellet
def fn(uN,uNPl=0.):
   ft=max(law.yieldForce(uN=uN,d0=d0,kn=kn,alpha=alpha),(uN-uNPl)*kn)
   if ft<0: return ft
   else: return law.adhesionForce(uN,uNPl,ka=ka)
pylab.plot(xxShort,[kn*x for x in xxShort],label='elastic')
pylab.plot(xx,[fn(x) for x in xx],label='plastic surface')
pylab.plot(xx,[fn(x,uNPl) for x in xx])
pylab.plot(xx,[fn(x,2*uNPl) for x in xx])
pylab.xlabel('normal displacement $u_n$')
pylab.ylabel('normal force $F_n$')

pylab.axhline(0,color='black',linewidth=2)
pylab.axvline(0,color='black',linewidth=2)
pylab.annotate('$-k_a u_n^{\\rm pl}$',(uNPl,law.adhesionForce(uNPl,2*uNPl,ka=ka)),xytext=(-10,10),textcoords='offset points',)
unloadX=1.5*uNPl
angle=52
pylab.annotate(r'elastic unloading',(unloadX,kn*(unloadX-uNPl)),xytext=(-70,0),textcoords='offset points',rotation=angle)
pylab.annotate(r'plastic loading',(.5*unloadX,fn(.5*unloadX)),xytext=(-70,5),textcoords='offset points',rotation=39)
pylab.annotate(r'adhesion',(uNPl,law.adhesionForce(uNPl,2*uNPl,ka=ka)),xytext=(-80,0),textcoords='offset points',rotation=15)
pylab.annotate(r'$-\frac{k_n d_0}{\alpha}\log\left(\alpha\frac{-u_n}{d_0}+1\right)$',(4*uNPl,fn(4*uNPl)),xytext=(10,-10),textcoords='offset points')
pylab.annotate(r'$k_n u_n$',(uNPl,uNPl*kn),xytext=(0,-10),textcoords='offset points')
pylab.plot(uNPl,0,'ro')
pylab.annotate(r'$u_n^{\rm pl}$',(uNPl,0),xytext=(10,-20),textcoords='offset points')
pylab.grid(True)