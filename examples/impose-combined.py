from woo.dem import *
from woo.core import *
from minieigen import *
import woo, math, numpy
S=woo.master.scene=Scene(fields=[DemField()])
S.dt=1e-6
s=Sphere.make((0,0,0),1)
tt=numpy.linspace(0,1,100)
# impose both aligned rotation and velocity
s.impose=VariableAlignedRotation(axis=0,timeAngVel=[(0,0),(1,1),(3,-1),(4,0)],wrap=True)+VariableVelocity3d(times=tt,vels=[(math.sin(t*2*math.pi),0,0) for t in tt],wrap=True)
S.dem.par.add(s)
S.engines=DemField.minimalEngines()
S.gl.demField.colorBy='angVel'
S.gl.demField.vecAxis='x'
