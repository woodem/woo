import woo
from woo.dem import *
from woo.core import *
from woo import *
from woo import utils
woo.master.usesApi=10101

S=woo.master.scene=Scene(fields=[DemField(gravity=(0,0,-10))])
m=utils.defaultMaterial()
S.dem.par.add([
    Facet.make([(0,0,0),(1,0,0),(0,1,0)],halfThick=0.3,mat=m),
    Sphere.make((.2,.2,1),.3,mat=m)
])
S.engines=DemField.minimalEngines(damping=.4)
S.dtSafety=.1
S.saveTmp()
