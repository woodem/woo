import woo, woo.core, woo.dem, woo.fem, woo.gl
import math, numpy
from minieigen import *
woo.master.usesApi=10103


S=woo.master.scene=woo.core.Scene(fields=[woo.dem.DemField(gravity=(0,0,-30))],dtSafety=.8)

xmax,ymax=1,1
xdiv,ydiv=10,10
mat=woo.dem.FrictMat(young=3e6,tanPhi=.55,ktDivKn=.2,density=3000)
ff=woo.pack.gtsSurface2Facets(woo.pack.sweptPolylines2gtsSurface([[(x,y,0) for x in numpy.linspace(0,xmax,num=xdiv)] for y in numpy.linspace(0,ymax,num=ydiv)]),flex=True,halfThick=.02,mat=mat)
S.dem.par.add(ff,nodes=True)

## important for below
for f in ff: f.shape.enableStress=True

# a few spheres falling onto the mesh
sp=woo.pack.SpherePack()
sp.makeCloud((.3,.3,.1),(.7,.7,.6),rMean=.3*xmax/xdiv,rRelFuzz=.5)
sp.toSimulation(S,mat=mat)

# set boundary conditions
for n in S.dem.nodes:
    if n.pos[0] in (0,xmax) or n.pos[1] in (0,ymax): n.dem.blocked='xyz'
    else: n.dem.blocked=''

S.engines=woo.dem.DemField.minimalEngines(damping=.4,verletDist=-0.01)+[woo.dem.IntraForce([woo.fem.In2_Membrane_ElastMat(bending=True)]),woo.dem.BoxOutlet(box=((-.1,-.1,-1),(1.1,1.1,1)),glColor=float('nan')),woo.core.PyRunner(50,'colorBySigma(S)')]

for n in S.dem.nodes: woo.dem.DemData.setOriMassInertia(n)

S.lab.sig=woo.core.ScalarRange(label='sigma')
S.ranges=[S.lab.sig]
def colorBySigma(S):
    for p in S.dem.par:
        if not isinstance(p.shape,woo.fem.Membrane): continue
        # get stress, converted to global tensor already
        sig=p.shape.stressCst(glob=True)[0,1] # look at sigma_xx only, for example
        # shorthand for:
        #sig=p.shape.node.loc2glob_rank2(p.shape.stressCst)[0,0]
        p.shape.color=S.lab.sig.norm(sig) # adjusts colorscale and returns value on the 0..1 scale, assign to particle color

S.gl=woo.gl.GlSetup(
    woo.gl.Gl1_DemField(shape='non-spheroids',colorBy='Shape.color',shape2=True,colorBy2='vel')
)


S.saveTmp()

import woo.qt
woo.qt.Controller()
woo.qt.View()

print('WARNING: this entire simulation is experimental and stresses might be bogus; please check with some analytically-solved example if the results are right!')
