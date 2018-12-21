import woo, woo.dem, woo.core, math
import numpy as np

# add a single stripe
def addStripe(S,y,**kw):
    y0,y1=y,y+.02
    z0,z1=0,.1
    x=0.001
    A,B,C,D=(x,y0,z0),(x,y1,z0),(x,y0,z1),(x,y1,z1)
    return woo.triangulated.quadrilateral(A,B,C,D,flex=True,halfThick=2e-3,div=(2,10),**kw)

S=woo.master.scene=woo.core.Scene(fields=[woo.dem.DemField(gravity=(-1,0,-10))])

model=woo.models.ContactModelSelector(name='linear',damping=.2,
    mats=[woo.dem.FrictMat(density=1e3,young=1e8,ktDivKn=.2,tanPhi=math.tan(.5))]
)

S.dem.par.add(woo.dem.Wall.make(0,axis=2,sense=+1,mat=model.mats[0]))
S.dem.par.add(sum([addStripe(S,y,mat=model.mats[0]) for y in np.arange(0,1,.03)],[]),nodes=True)

# fix upper edge of stripes
for n in S.dem.nodes:
    if n.pos[2]==.1:
        n.dem.blocked='xyzXYZ'
    else: n.dem.blocked=''
# engines, standard plus membrane stuff; disable bending
S.engines=woo.dem.DemField.minimalEngines(model=model)+[
    woo.dem.IntraForce([woo.fem.In2_Membrane_ElastMat(bending=False)]),
]

# lump mass and inertia of membranes to nodes (this is not done automatically)
for p in S.dem.par:
    for n in p.shape.nodes:
        m,I,rot=p.shape.lumpMassInertia(n,p.mat.density)
        n.dem.mass+=m
        n.dem.inertia+=I.diagonal()
    # disable numerical instability warning for membranes 
    if hasattr(p.shape,'noWarnExcessRot'): p.shape.noWarnExcessRot=True

# add a few spheres which will fall onto stripes
sp=woo.pack.SpherePack()
sp.makeCloud((.1,.1,.0),(.2,.9,.1),rMean=.02,rRelFuzz=.3)
sp.toSimulation(S,mat=model.mats[0])
