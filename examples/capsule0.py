import woo.core, woo.utils, woo
from woo.dem import *
from woo.gl import *
from minieigen import *
from math import *
import numpy
from random import random

woo.master.usesApi=10102

S=woo.master.scene=woo.core.Scene(fields=[DemField(gravity=(0,0,-10))])

mat=FrictMat(young=1e6,density=3200)

generators=[
    PsdCapsuleGenerator(psdPts=[(.15,0),(.25,.8),(.3,1.)],discrete=False,mass=True,shaftRadiusRatio=(1.,2.)),
    # PsdEllipsoidGenerator(psdPts=[(.15,0),(.3,1.)],discrete=False,mass=True,axisRatio2=(.2,.6),axisRatio3=(.2,.6)),
    PsdClumpGenerator(psdPts=[(.15,0),(.25,1.)],discrete=False,mass=True,
        clumps=[
            SphereClumpGeom(centers=[(0,0,-.06),(0,0,0),(0,0,.06)],radii=(.05,.08,.05),scaleProb=[(0,1.)]),
            SphereClumpGeom(centers=[(-.03,0,0),(0,.03,0)],radii=(.05,.05),scaleProb=[(0,.5)]),
        ],
    ),
    PsdSphereGenerator(psdPts=[(.15,0),(.25,.8),(.3,1.)],discrete=False,mass=True),
]


S.dem.par.add([
    woo.utils.facet([(.6,0,.3),(0,.6,.3),(-.6,0,.3)],halfThick=.1,wire=False),
    woo.utils.wall(0,axis=2,sense=1),
    woo.utils.infCylinder((.33,.33,0),radius=.3,axis=1),
])
S.dem.par[-1].angVel=(0,.5,0)
S.dtSafety=.2
S.engines=woo.utils.defaultEngines(damping=.4,dynDtPeriod=10)+[BoxInlet(box=((-1,-1,1+i*2),(1,1,3+i*2)),stepPeriod=100,maxMass=3e3,maxNum=-1,massRate=0,maxAttempts=100,attemptPar=50,atMaxAttempts=BoxInlet.maxAttWarn,generator=generators[i],materials=[mat]) for i in range(len(generators))]

# S.throttle=.03
S.saveTmp()
S.gl=GlSetup(Gl1_DemField(colorBy='radius'),Gl1_Sphere(quality=3),Renderer(engines=False,iniViewDir=Vector3(-0.38,-0.554,-0.74),iniUp=Vector3(-0.4746,-0.582,0.66)))
