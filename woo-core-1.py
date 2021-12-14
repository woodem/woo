import woo, woo.plot, woo.utils
from woo.dem import *
from woo.core import *
S=Scene(fields=[DemField(gravity=(0,0,-10))])
S.dem.par.add(Sphere.make((0,0,0),1));
S.engines=[Leapfrog(damping=.4,reset=True),PyRunner('S.plot.autoData()')]
S.plot.plots={'i=S.step':('**S.energy','total energy=S.energy.total()',None,'rel. error=S.energy.relErr()')}
S.trackEnergy=True
S.run(500,True)
S.plot.legendLoc=('lower left','upper right')
S.plot.plot()