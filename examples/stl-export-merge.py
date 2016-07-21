from woo.core import *
from woo.dem import *
from minieigen import *
import woo.triangulated
import woo.log
woo.log.setLevel('',woo.log.TRACE)
S=woo.master.scene=Scene(fields=[DemField(par=[
    Sphere.make((0,0,0),.1),
    Sphere.make((.2,0,0),.15),
    Capsule.make((.2,.15,0),radius=.1,shaft=.2),
    # Ellipsoid.make((0,.5,0),semiAxes=(.1,.2,.1),ori=Quaternion((.3,.5,.1),33.4)),
    #Capsule.make((.5,.5,0),radius=.02,shaft=.2,ori=((1,2,3),4))
])])
woo.triangulated.spheroidsToSTL('/tmp/woo-stl-export-merge.stl',S.dem,tol=1e-3,merge=True)

