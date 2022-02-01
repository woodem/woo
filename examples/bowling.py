from woo.core import *
from woo.dem import *
from woo.fem import *
import woo, woo.triangulated
from math import pi
from minieigen import *
woo.master.usesApi=10101
S=woo.master.scene=Scene(fields=[DemField(gravity=(0,0,-10),loneMask=0)],dtSafety=0.9)
S.engines=DemField.minimalEngines(damping=.001)+[IntraForce([In2_Tet4_ElastMat(),In2_Facet(),In2_Membrane_FrictMat(bending=True)])]

mPin=FrictMat(density=1000,young=200e6)
mFloor=mPin
mBall=FrictMat(density=2500,young=200e6)
rBall=.5*0.2159
mask=0b001
halfThick=0.005
# geometry as per http://www.bowlexcel.com/bowling-tips-a-typical-pindeck-setup.html
for pIn in [(0,0),(-6,10.25),(6,10.25),(-12,20.5),(0,20.5),(12,20.5),(-18,30.75),(-6,30.75),(6,30.75),(18,30.75)]:
    nn,pp=woo.utils.importNmesh('bowling-pin.nmesh',mat=mPin,mask=mask,trsf=lambda v: Vector3(pIn[0],pIn[1],0)*woo.unit['in']+Vector3(v[0],v[2],v[1]+halfThick),dem=S.dem,surfHalfThick=halfThick)
    for p in pp:
        if isinstance(p.shape,Facet): p.shape.visible=False
        else: p.shape.wire=False
S.dem.par.add(Wall.make(0,sense=1,axis=2,mat=mFloor,glAB=((-1,-.6),(1,1.2))))
S.lab.collider.noBoundOk=True
s=Sphere.make(center=(0,-.3,rBall),radius=rBall,mat=mBall)
s.vel=(0,25*woo.unit['km/h'],0)
s.angVel=(-(25*woo.unit['km/h'])/rBall,0,0)
S.dem.par.add(s)
for n in S.dem.nodes: DemData.setOriMassInertia(n)
S.engines+=[VtkExport(what=VtkExport.spheres|VtkExport.mesh|VtkExport.tri,out='/tmp/bowl/',stepPeriod=100)]
