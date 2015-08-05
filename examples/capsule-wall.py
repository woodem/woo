import woo, woo.core, woo.dem, woo.utils, woo.gl
from minieigen import *
from math import *
woo.master.usesApi=10101

S=woo.master.scene=woo.core.Scene(dtSafety=.1,throttle=0.01,fields=[woo.dem.DemField(gravity=(0,0,-10))])
mat=woo.dem.FrictMat(young=1e6,ktDivKn=.2,density=3e3)
S.dem.par.add([woo.utils.capsule((0,0,.5),radius=.3,shaft=.8,ori=Quaternion((0,1,0),-pi/8.),wire=True,mat=mat),woo.utils.wall(0,axis=2,sense=1,mat=mat)])
S.engines=woo.utils.defaultEngines(dynDtPeriod=100,damping=.4)
S.saveTmp()

# view setup
S.gl.demField.cPhys=True
S.gl.cPhys.relMaxRad=.1
S.gl.renderer.allowFast=False
S.gl.renderer.iniViewDir=(0,-1,0)
S.gl.capsule.smooth=True

