import woo, woo.pre
from minieigen import *
pre=woo.pre.depot.CylDepot(htDiam=(.1,.1),unbE=0.01)
pre.model.mats[0].young=20000
S=woo.master.scene=pre()
S.run(wait=True)
surf=woo.triangulated.surfParticleIdNormals(S.dem,box=((-.05,-.05,0),(.05,.05,.15)),r=.03)
S.gl.demField.shape='spheroids'
S.gl.demField.shape2=False
S.gl.demField.colorBy='Shape.color'
for p in S.dem.par: p.shape.color=0
# color by number of surface faces
for s in surf:
    sumNorm=sum(surf[s],Vector3.Zero).normalized()
    c=.8*max(sumNorm[2],0)
    S.dem.par[s].shape.nodes[0].rep=woo.gl.VectorGlRep(val=sumNorm,relSz=.06)
    # c=.2*sum([1 for n in nn if n.normalized()[2]>.8])
    # c=min(.1*len(surf[s]),.9)
    S.dem.par[s].shape.color=c
