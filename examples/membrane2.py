#
# TODO: S.loneGroup
#
from woo.core import *
from woo.dem import *
from woo.fem import *
import woo
import woo.gl
import math
from math import pi
from minieigen import *
woo.master.usesApi=10102
import woo.log
#woo.log.setLevel('In2_Membrane_ElastMat',woo.log.TRACE)
#woo.log.setLevel('DynDt',woo.log.DEBUG)


S=woo.master.scene=Scene(fields=[DemField(gravity=(0,0,-30))])

S.gl.node.wd=2
S.gl.node.len=.05
S.gl.membrane.uScale=1.
S.gl.membrane.relPhi=.1
S.gl.membrane.phiSplit=False
S.gl.membrane.phiWd=5
S.gl.demField.colorBy='vel'


if 1:
	import woo.pack, woo.utils, numpy
	scenario=['youtube','plain','inverse'][0]
	if scenario=='youtube':
		# http://www.youtube.com/watch?v=KmQWD_MfR8M
		xmax,ymax=1,1
		xdiv,ydiv=10,10
		mat=woo.utils.defaultMaterial()
		mat.young*=.1
		ff=woo.pack.gtsSurface2Facets(woo.pack.sweptPolylines2gtsSurface([[(x,y,0) for x in numpy.linspace(0,xmax,num=xdiv)] for y in numpy.linspace(0,ymax,num=ydiv)]),flex=True,halfThick=.01,mat=mat)
		S.dem.par.add(ff)
		S.gl(
			woo.gl.Renderer(dispScale=(10,10,10)),
			woo.gl.Gl1_Membrane(node=False,refConf=False,uScale=0.,relPhi=0.),
			woo.gl.Gl1_DemField(shape='nsph',colorBy='disp',vecAxis='norm',colorBy2='vel')
		)
		S.gl.demField.colorRange2.mnmx=(0,2.)
	else:
		S.gl.renderer.dispScale=(10,10,100)
		S.gl.renderer.scaleOn=True
		S.gl.demField.nodes=True
		S.gl.demField.glyphRelSz=.3
		S.gl.membrane.node=True
		## this triggers weirdness in rotation computation!! (quaternion wrapping around?!)
		if scenario=='inverse':
			f=woo.utils.facet([(0,0,0),(0,.2,0),(.2,0,0)],flex=True) #!!!
			import woo.log
			#woo.log.setLevel('In2_Membrane_ElastMat',woo.log.TRACE)
			woo.log.setLevel('Membrane',woo.log.TRACE)
		else: f=woo.utils.facet([(.2,.1,0),(0,.2,0),(0,0,0)],flex=True) 
		S.dem.par.add(f)
	for n in S.dem.nodes:
		n.dem.blocked=''
		if n.pos[0]==0 or (n.pos[1]==0 and scenario=='youtube'): n.dem.blocked='xyzXYZ'
		r=.02; V=(4/3.)*pi*r**3
		n.dem.inertia=1e3*(2/5.)*V*r**2*Vector3(1,1,1)
		n.dem.mass=V*1e3
	# this would be enough without spheres
	#S.engines=[Leapfrog(reset=True,damping=.05),IntraForce([In2_Membrane_ElastMat(thickness=.01)])]
	S.engines=[
		Leapfrog(reset=True,damping=.2),
		InsertionSortCollider([Bo1_Sphere_Aabb(),Bo1_Facet_Aabb()],verletDist=0.01),
		ContactLoop([Cg2_Sphere_Sphere_L6Geom(),Cg2_Facet_Sphere_L6Geom()],[Cp2_FrictMat_FrictPhys()],[Law2_L6Geom_FrictPhys_IdealElPl()],applyForces=False), # forces are applied in IntraForce
		IntraForce([In2_Membrane_ElastMat(thickness=.1,bending=True,bendThickness=.03),In2_Sphere_ElastMat()]),
		DynDt(stepPeriod=100),
		# VtkExport(out='/tmp/membrane',stepPeriod=100,what=VtkExport.spheres|VtkExport.mesh)
	]
	
	# a few spheres falling onto the mesh
	if scenario=='youtube':
		sp=woo.pack.SpherePack()
		sp.makeCloud((.3,.3,.1),(.7,.7,.3),rMean=.3*xmax/xdiv,rRelFuzz=.3,periodic=False)
		sp.toSimulation(S,mat=FrictMat(young=1e6,density=3000))
	
	# split the plate along the diagonal
	for n in [n for n in S.dem.nodes if n.pos[0]==n.pos[1]]:
		# make the other end of the diagonal fixed as well, just for the fun of it
		if n.pos[1]==1.: n.dem.blocked='xyzXYZ'
		S.dem.splitNode(n,[p for p in n.dem.parRef if p.shape.getCentroid()[0]>p.shape.getCentroid()[1]])

	S.saveTmp()

import woo.qt
woo.qt.Controller()
woo.qt.View()
