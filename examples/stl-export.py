from woo.core import *
from woo.dem import *
from minieigen import *
import woo.triangulated
S=woo.master.scene=Scene(fields=[DemField(par=[Sphere.make((0,0,0),.1),Capsule.make((.5,0,0),radius=.1,shaft=.2),Ellipsoid.make((0,.5,0),semiAxes=(.1,.2,.1),ori=Quaternion((.3,.5,.1),33.4)),Capsule.make((.5,.5,0),radius=.02,shaft=.2,ori=((1,2,3),4))])])
woo.triangulated.spheroidsToSTL('/tmp/woo-stl-export.stl',S.dem,tol=1e-3)
