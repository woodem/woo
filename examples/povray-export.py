import woo
from woo import utils,pack
from woo.dem import *
from woo.core import *
from minieigen import *
import gts
woo.master.usesApi=10101
woo.master.scene=S=Scene(fields=[DemField(gravity=(0,0,-10))],dtSafety=.5)

mat=utils.defaultMaterial()

triBit=DemField.defaultHighestBit<<1
cylBit=triBit<<1
triMask=DemField.defaultStaticMask|triBit
cylMask=DemField.defaultMovableMask|cylBit

S.dem.par.add([
    Wall.make(-2,axis=2,sense=0,mat=mat,glAB=((-5,-1),(12,8))),
    Facet.make([(-2,-2,0),(2,0,0),(0,2,0)],halfThick=.9,mat=mat,fixed=True,wire=True),
    InfCylinder.make((0,-2,-.5),radius=1.,axis=0,mat=mat,angVel=(1,0,0),mask=cylMask),
    InfCylinder.make((5,0,0),radius=1.5,axis=1,glAB=(-3,5),mat=mat,angVel=(0,.5,0),mask=cylMask),
    Sphere.make((5,4,3),1.,mat=mat),
    Capsule.make((8,6,1),radius=.8,shaft=2,ori=Quaternion(1,(0,.1,1)),mat=mat),
    Ellipsoid.make((0,6,1),semiAxes=(1.1,1.7,.9),ori=Quaternion(1,(.4,.4,1)),mat=mat)
],nodes=True)

surf=gts.sphere(3)
surf.scale(5.,5.,5.)
surf.translate(0,0,4)
S.dem.par.add(woo.pack.gtsSurface2Facets(surf,mask=triMask))

ex=POVRayExport(out='/tmp/pp0',stepPeriod=10,initRun=True,wallTexture='wall',cylCapTexture='flat')
ex.masks=[triBit,cylBit]+ex.masks
ex.textures=["tri","cyl"]+ex.textures

S.engines=DemField.minimalEngines(damping=.2)+[ex]

S.saveTmp()
S.one()
S.run(2000,wait=True)
import subprocess, os.path
subprocess.call(['povray','+W1500','+H1000','+P',ex.out+"_master.pov"],cwd=os.path.dirname(ex.out))
