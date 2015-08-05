from woo.core import *
from woo.dem import *
from woo.fem import *
import woo
import woo.gl
import math
from math import pi
from minieigen import *

S=woo.master.scene=Scene(fields=[DemField()])
S.gl.demField.nodes=True
S.gl.node.wd=4
S.gl.node.len=.1
S.gl.membrane.phiSplit=True
S.gl.membrane.phiWd=10
S.gl.membrane.arrows=True

#nn=[Node(pos=(1,0,0)),Node(pos=(0,1,0)),Node(pos=(0,0,1))]
nn=[Node(pos=(1,0,0)),Node(pos=(0,1,0)),Node(pos=(0,0,1))]
for n in nn:
	n.dem=DemData(inertia=(1,1,1))
	n.dem.blocked='xyzXYZ'
	rotvec=Vector3.Random()
	n.ori=Quaternion(rotvec.norm(),rotvec.normalized())

#nn[0].dem.vel=(1.,0,0)
### orientation is computed WRONG:
### global x-directed angVel contributes only to LOCAL phi_x on the facet!!!
#nn[0].ori=Quaternion(Matrix3(Vector3.UnitY,Vector3.UnitZ,Vector3.UnitX,cols=True))
#nn[0].ori=Quaternion(Vector3.UnitZ,pi/2)
#nn[0].dem.angVel=(0,0,10)
#nn[1].dem.vel=(0,0,0)
#nn[2].dem.vel=(0,0,0)

#for i,n in enumerate(nn):
#	#n.dem.vel=Vector3.Random()
#	n.dem.angVel=2.*(n.ori*Vector3.Unit(i))
nn[2].dem.angVel=2*(nn[2].ori*Vector3.UnitZ)

#nn[0].ori=Quaternion(Matrix3(-Vector3.UnitY,-Vector3.UnitZ,Vector3.UnitX))
#nn[0].dem.angVel=10.*Vector3.UnitY

S.dem.par.add(Particle(shape=Membrane(nodes=nn),material=FrictMat()))
for n in nn: n.dem.addParRef(S.dem.par[-1])

# ff=S.dem.par[0].shape
# f.setRefConf() #update()
S.engines=[Leapfrog(reset=True),PyRunner(1,'S.dem.par[0].shape.update(S.dt)')]
S.dt=1e-5
S.saveTmp()

import woo.qt
woo.qt.Controller()
woo.qt.View()
