from __future__ import print_function
import woo.dem, woo.core, woo.log, woo.triangulated, woo.gl, woo.qt
woo.master.usesApi=10102
from minieigen import *
S=woo.master.scene=woo.core.Scene(fields=[woo.dem.DemField(gravity=(0,0,-10))])
box=AlignedBox3((0,0,0),(1.5,1.5,3))
S.dem.par.add(woo.dem.Wall.makeBox(box))
S.engines=woo.dem.DemField.minimalEngines(damping=.3)+[
	woo.dem.BoxInlet(
		box=box,
		generator=woo.dem.PsdSphereGenerator(psdPts=[(.05,0),(.1,.4),(.2,1)],discrete=False,mass=True),
		# spatialBias=woo.dem.PsdAxialBias(psdPts=[(.05,0),(.1,.4),(.2,1)],axis=2,fuzz=.1,discrete=False),
		maxMass=-1,maxNum=-1,massRate=0,maxAttempts=5000,materials=[woo.dem.FrictMat(density=1000,young=1e6,tanPhi=.9)],nDo=1
	),
	woo.core.PyRunner(50,'recomputePorosity(S)')
]

def recomputePorosity(S):
	# this computes radical Delaunay tesselation and returns [(id,(x,y,z),porosity),...]
	# this is what the tesselation looks like: http://math.lbl.gov/voro++/examples/
	# quite inefficient to be run periodically, this is just a showcase
	poro=woo.triangulated.porosity(S.dem,box=box)
	# color particles by local porosity
	for p in poro: S.dem.par[p[0]].shape.color=p[2]

# generate particles
S.one() 
# setup the view
S.gl.demField.colorBy='Shape.color'
r=S.gl.demField.colorRange; r.hidden=False; r.mnmx=(0,1); r.label='porosity'
# clip in the middle
S.gl.renderer.clipPlanes=[woo.core.Node(pos=(0,.5,0),ori=((0,.7071,.7071),3.1415),rep=woo.core.NodeVisRep())]
woo.qt.View()

# plot with matplotlib
if 0:
	xx,yy,zz,pp=[p[1][0] for p in poro],[p[1][1] for p in poro],[p[1][2] for p in poro],[p[2] for p in poro]
	import matplotlib.pyplot as plt
	import mpl_toolkits.mplot3d, matplotlib.lines
	fig=plt.figure()
	ax=fig.add_subplot(111,projection='3d')
	s=ax.scatter(xx,yy,zz,c=pp)
	fig.colorbar(s)
	plt.show()
