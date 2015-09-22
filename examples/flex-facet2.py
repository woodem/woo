from woo.core import *
from woo.dem import *
from woo.fem import *
import woo
woo.master.usesApi=10102
import woo.gl
import math
from math import pi
from minieigen import *

import woo.log
woo.log.setLevel('Membrane',woo.log.TRACE)

S=woo.master.scene=Scene(fields=[DemField()])

S.gl.demField.nodes=True
S.gl.node.wd=4
S.gl.node.len=.2
S.gl(woo.gl.Gl1_Membrane(phiSplit=True,relPhi=1,phiWd=10,arrows=False,wire=True))
#nn=[Node(pos=(1,0,0)),Node(pos=(0,1,0)),Node(pos=(0,0,1))]
nn=[Node(pos=(1,0,0)),Node(pos=(0,1,0)),Node(pos=(0,0,0))]
for n in nn:
    n.dem=DemData(inertia=(1,1,1))
    n.dem.blocked='xyzXYZ'
    rotvec=Vector3.Random(); n.ori=Quaternion(rotvec.norm(),rotvec.normalized())

#nn[0].dem.vel=(1.,0,0)
### orientation is computed WRONG:
### global x-directed angVel contributes only to LOCAL phi_x on the facet!!!
#nn[0].ori=Quaternion(Matrix3(Vector3.UnitY,Vector3.UnitZ,Vector3.UnitX,cols=True))
#nn[0].ori=Quaternion(Vector3.UnitZ,pi/2)
#nn[0].dem.angVel=(0,0,10)
#nn[1].dem.vel=(0,0,0)
#nn[2].dem.vel=(0,0,0)

#for i,n in enumerate(nn):
#    #n.dem.vel=Vector3.Random()
#    n.dem.angVel=2.*(n.ori*Vector3.Unit(i))
#nn[2].dem.angVel=.2*Vector3.UnitX # 2*(nn[2].ori*Vector3.UnitX)
nn[0].dem.vel=.2*Vector3(0,0,2.)

#nn[0].ori=Quaternion(Matrix3(-Vector3.UnitY,-Vector3.UnitZ,Vector3.UnitX))
#nn[0].dem.angVel=10.*Vector3.UnitY

S.dem.par.add(Particle(shape=Membrane(nodes=nn),material=FrictMat()),nodes=True)
for n in nn: n.dem.addParRef(S.dem.par[-1])

ff=S.dem.par[0].shape
ff.setRefConf() #update()
# for n in nn: S.dem.nodesAppend(n)
# S.engines=[Leapfrog(reset=True),PyRunner(1,'S.dem.par[0].shape.update()')]
S.engines=[
    Leapfrog(reset=True,damping=.1),
    InsertionSortCollider([Bo1_Sphere_Aabb(),Bo1_Facet_Aabb()],verletDist=0.01),
    ContactLoop([Cg2_Sphere_Sphere_L6Geom(),Cg2_Facet_Sphere_L6Geom()],[Cp2_FrictMat_FrictPhys()],[Law2_L6Geom_FrictPhys_IdealElPl()],applyForces=False), # forces are applied in IntraForce
    IntraForce([In2_Membrane_ElastMat(thickness=.01,bending=True,bendThickness=.2),In2_Sphere_ElastMat()]),
    # VtkExport(out='/tmp/membrane',stepPeriod=100,what=VtkExport.spheres|VtkExport.mesh)
]
S.dt=1e-5
S.saveTmp()

import woo.qt
woo.qt.Controller()
woo.qt.View()
nc=ff.node
n0=ff.nodes[0]
