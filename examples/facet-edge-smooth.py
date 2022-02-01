import woo.core, woo.dem, woo.triangulated, woo.log
import math
from minieigen import *
woo.log.setLevel('Facet',woo.log.TRACE)
S=woo.master.scene=woo.core.Scene(fields=[woo.dem.DemField()],engines=woo.dem.DemField.minimalEngines())
S.dem.par.add(woo.triangulated.sweep2d([(0,0),(1,0),(2,-1),(3,0),(4,0),(4,-1),(5,-1),(5,0),(6,0)],zz=(0,1),node=woo.core.Node(ori=Quaternion((0,0,1),math.pi/2)*Quaternion((1,0,0),math.pi/2.))))
for p in S.dem.par:
    if isinstance(p.shape,woo.dem.Facet):
        p.shape.computeNeighborAngles()
        print(p.id,p.shape.n21lim)
# s=woo.dem.Sphere.make((.5,-.05,.45),radius=.5)
s=woo.dem.Sphere.make((.5,.255,.45),radius=.5,mat=woo.dem.FrictMat(tanPhi=0,young=1e7))
s.blocked='xyzXYZ'
s.vel=(0,1,0)
S.dem.par.add(s)
S.dt=1e-3
S.throttle=2e-2


S.gl.sphere.scale=.1

S.gl.demField.line=True

S.saveTmp()
S.one()
