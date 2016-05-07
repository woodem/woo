from woo.core import *; from woo.dem import *; import woo; from minieigen import *; from math import pi
woo.master.usesApi=10103
nn=Node(pos=(0,0,1.1),ori=Quaternion((0,1,0),5*pi/12))
S=woo.master.scene=Scene(fields=[DemField(gravity=(0,0,0),
    #par=[Wall.make(0,sense=1,axis=2,mat=woo.utils.defaultMaterial())]
    )],
    engines=DemField.minimalEngines(damping=.2)+[
        ArcInlet(glColor=.4,massRate=10,stepPeriod=200,node=nn,
            cylBox=AlignedBox3((.9,0,0),(1,2*pi,.3)),
            generator=PsdSphereGenerator(psdPts=[(.02,0),(.03,.2),(.04,.9),(.05,1.)]),
            materials=[woo.utils.defaultMaterial()],
            shooter=ArcShooter(node=nn,vRange=(.05,.2),azimRange=(.9,1.1),elevRange=(.8,1.2))),
        BoxOutlet(inside=False,stepPeriod=200,box=((-.5,-1.5,-.1),(1.5,1.5,2.3)),glColor=.3),
        Tracer(stepPeriod=200)
    ]
)
S.saveTmp()

