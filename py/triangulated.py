from minieigen import *
import woo.pack
import numpy
import math

from woo._triangulated import *

_docInlineModules=(woo._triangulated,)

def cylinder(A,B,radius,div=20,axDiv=1,capA=False,capB=False,masks=None,wallCaps=False,angVel=0,realRot=False,fixed=True,**kw):
	'''Return triangulated cylinder, as list of facets to be passed to :obj:`woo.dem.ParticleContainer.append`. ``**kw`` arguments are passed to :obj:`woo.pack.gtsSurface2Facets` (and thus to :obj:`woo.utils.facet`).

:param angVel: axial angular velocity of the cylinder
:param realRot: if true, impose :obj:`~woo.dem.StableCircularOrbit` on all cylinder nodes; if false, only assign :obj:`woo.dem.Facet.fakeVel` is assigned. In both cases, :obj:`~woo.dem.DemData.angVel` is set on cap walls (with *wallCaps*).
:param axDiv: divide the triangulation axially as well; the default creates facets spanning between both bases. If negative, it will attempt to have that ratio with circumferential division, e.g. specifying -1 will give nearly-square division.
:param wallCaps: create caps as walls (with :obj:`woo.dem.wall.glAB` properly set) rather than triangulation; cylinder axis *must* be aligned with some global axis in this case, otherwise and error is raised.
:param masks: mask values for cylinder, capA and capB respectively; it must be either *None* or a 3-tuple (regardless of whether caps are created or not); ``**kw`` can be used to specify ``mask`` which is passed to :obj:`woo.dem.Facet.make`, but *masks* will override this, if given.
:returns: List of :obj:`particles <woo.dem.Particle>` building up the cylinder. Caps (if any) are always at the beginning of the list, triangulated perimeter is at the end.
	'''
	cylLen=(B-A).norm()
	ax=(B-A)/cylLen
	axOri=Quaternion(); axOri.setFromTwoVectors(Vector3.UnitX,ax)

	thetas=numpy.linspace(0,2*math.pi,num=div,endpoint=True)
	yyzz=[Vector2(radius*math.cos(th),radius*math.sin(th)) for th in thetas]
	if axDiv<0:
		circum=2*radius*math.pi
		axDiv=int(abs(axDiv)*cylLen/(circum/div))
	xx=numpy.linspace(0,cylLen,num=max(axDiv+1,2))
	xxyyzz=[[A+axOri*Vector3(x,yz[0],yz[1]) for yz in yyzz] for x in xx]
	# add caps, if needed; the points will be merged automatically in sweptPolylines2gtsSurface via threshold
	caps=[]
	if wallCaps:
		# determine cylinder orientation
		cAx=-1
		for i in (0,1,2):
			 if abs(ax.dot(Vector3.Unit(i)))>(1-1e-6): cAx=i
		if cAx<0: raise ValueError("With wallCaps=True, cylinder axis must be aligned with some global axis")
		sign=int(math.copysign(1,ax.dot(Vector3.Unit(cAx)))) # reversed axis
		a1,a2=((cAx+1)%3),((cAx+2)%3)
		glAB=AlignedBox2((A[a1]-radius,A[a2]-radius),(A[a1]+radius,A[a2]+radius))
		if capA:
			caps.append(woo.utils.wall(A,axis=cAx,sense=sign,glAB=glAB,**kw))
			if angVel!=0: caps[-1].shape.nodes[0].dem.angVel[cAx]=sign*angVel
		if capB:
			caps.append(woo.utils.wall(B,axis=cAx,sense=-sign,glAB=glAB,**kw))
			if angVel!=0: caps[-1].shape.nodes[0].dem.angVel[cAx]=sign*angVel
	else:
		# add as triangulation
		if capA: xxyyzz=[[A+Vector3.Zero for yz in yyzz]]+xxyyzz
		if capB: xxyyzz+=[[B+Vector3.Zero for yz in yyzz]]
	ff=woo.pack.gtsSurface2Facets(woo.pack.sweptPolylines2gtsSurface(xxyyzz,threshold=min(radius,cylLen)*1e-4),fixed=fixed,**kw)
	if angVel!=0: 
		if not realRot:
			for f in ff:
				f.shape.fakeVel=-radius*angVel*f.shape.getNormal().cross(ax)
		else:
			qq=Quaternion(); qq.setFromTwoVectors(Vector3.UnitZ,ax)
			orbit=woo.dem.StableCircularOrbit(node=woo.core.Node(pos=A,ori=qq),omega=angVel,radius=radius)
			for f in ff:
				for n in f.shape.nodes:
					n.dem.impose=orbit
	# override masks of particles
	if masks:
		for f in ff: f.mask=masks[0] # set the cylinder as if everywhere first
		if wallCaps:
			if capA: caps[0].mask=masks[1]
			if capB: caps[-1].masks[2]
		else:
			if capA:
				for f in ff:
					if sum([1 for n in f.shape.nodes if n.pos==A]): f.mask=masks[1]
			if capB:
				for f in ff:
					if sum([1 for n in f.shape.nodes if n.pos==B]): f.mask=masks[2]
	return caps+ff

def ribbedMill(A,B,radius,majNum,majHt,majTipAngle,minNum=0,minHt=0,minTipAngle=0,div=20,**kw):
	'''

:param majNum: number of major ribs (parallel with rotation axis)
:param majHt: height of major ribs
:param majTipAngle: angle on the major rib tip
:param minNum: number of minor ribs (around the cylinder, in plane perpendicular to the rotation axis)
:param minHt: height of minor ribs
:param minTipAngle: angle on the minor rib tip
:returns: tuple ``[particles],centralNode``, which can be directly used in ``S.dem.par.addClumped(*woo.triangulated.ribbedMill(...))``. ``centralNode`` is local coordinate system positioned in ``A`` and of which :math:`x`-axis is rotation axis of the mill. Assigning ``dem.angVel=centralNode.ori*(0,0,w)`` will make the mill rotate around the axis with the angular velocity ``w``. ``centralNode`` has automatically ClumpData assigned, with :obj:`~woo.dem.DemData.blocked` equal to ``xyzXYZ`.
'''
	A,B=Vector3(A),Vector3(B)
	cylLen=(B-A).norm()
	ax=(B-A)/cylLen
	axOri=Quaternion(); axOri.setFromTwoVectors(Vector3.UnitZ,ax)
	cs=woo.core.Node(pos=A,ori=axOri,dem=woo.dem.ClumpData(blocked='xyzXYZ'))
	locCyl=lambda r,th,z: cs.loc2glob(woo.comp.cyl2cart(Vector3(r,th,z)))
	locCart=cs.loc2glob

	majPeri=2*majHt*math.tan(.5*majTipAngle) # length of a maj on the perimeter of the mill
	majAngle=majPeri/radius # angle of one rib from the axis of the mill
	interMajAngle=2*math.pi/majNum
	interMajNum=max(2,int(math.ceil(div*1./majNum)))
	majRad=radius-majHt
	pts=[]; thMin=0
	for i in range(0,majNum):
		thMin+=interMajAngle
		thMax=thMin+interMajAngle-majAngle
		thTip=thMax+.5*majAngle
		# the circular parts spanning from thMin to thMax
		thetas=numpy.linspace(thMin,thMax,interMajNum,endpoint=True)
		for th0 in thetas: pts.append(locCyl(radius,th0,0))
		# tip of the major rib
		pts.append(locCyl(majRad,thTip,0))
	# close the curve
	pts+=[pts[0]]
	# make the second contour, just shifted by millDp; ppts contains both
	ppts=[pts,[p+axOri*Vector3(0,0,cylLen) for p in pts]]
	millPar=woo.pack.gtsSurface2Facets(woo.pack.sweptPolylines2gtsSurface(ppts,threshold=.001*majHt),**kw)
	# minor ribs
	minPar=[]
	if minNum>0:
		minHalfWd=minHt*math.tan(.5*minTipAngle)
		thetas=numpy.linspace(0,2*math.pi,majNum+1,endpoint=True)
		for i,xx in enumerate(numpy.linspace(0,cylLen,num=minNum,endpoint=True)):
			pts=[]
			for th in thetas:
				A,B,C=locCyl(radius,th,xx-.5*minHalfWd),locCyl(radius-minHt,th,xx),locCyl(radius,th,xx+.5*minHalfWd)
				if i==0: pts.append((B,C))
				elif i==minNum-1: pts.append((A,B))
				else: pts.append((A,B,C))
			# one rib is done all around
			minPar+=woo.pack.gtsSurface2Facets(woo.pack.sweptPolylines2gtsSurface(pts,threshold=0.001*minHt),**kw)
	return millPar+minPar,cs

def quadrilateral(A,B,C,D,size=0,div=Vector2i(0,0),**kw):
	'Return triangulated `quadrilateral <http://en.wikipedia.org/wiki/Quadrilateral>`__ (or a `skew quadrilateral <http://en.wikipedia.org/wiki/Skew_polygon>`__), when points are not co-planar), where division size is at most *size* (absolute length) or *div* (number of subdivisions in the AB--CD and AC--BD directions).'
	if sum(div)>0:
		if min(div)<1: raise ValueError('Both components of div must be positive')
		if size>0: raise ValueError('only one of *div* or *size* may be given (not both)')
	else:
		l1,l2=min((A-C).norm(),(D-B).norm()),min((A-B).norm(),(C-D).norm())
		if size!=0: div=Vector2i(int(max(2,math.ceil(l1/size))),int(max(2,math.ceil(l2/size))))
		else: div=Vector2i(2,2)
	AB,AC,CD,BD=B-A,C-A,D-C,D-B
	aabb,aacc=numpy.linspace(0,1,div[0]+1),numpy.linspace(0,1,div[1]+1)
	pts=[[A+ac*AC+ab*(B+ac*BD-(A+ac*AC)) for ac in aacc] for ab in aabb]
	return woo.pack.gtsSurface2Facets(woo.pack.sweptPolylines2gtsSurface(pts),**kw)

def box(dim,center,**kw):
	'''Return box created from facets. ``**kw`` args are passed to :obj:`woo.dem.Facet.make`. If facets have thickness, node masses and inertia will be computed automatically.

:param dim: dimensions along x, y, z axes;
:param center: a :obj:`minieigen:Vector3` (box aligned with global axes) or :obj:`woo.core.Node` (local coordinate system) giving the center of the box;
'''
	if isinstance(center,Vector3): c=woo.core.Node(pos=center)
	else: c=center
	nn=[woo.core.Node(pos=c.loc2glob(Vector3(.5*sgn[0]*dim[0],.5*sgn[1]*dim[1],.5*sgn[2]*dim[2]))) for sgn in ((-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1))]
	indices=[(0,2,1),(0,3,2),(0,1,5),(0,5,4),(0,4,3),(3,4,7),(1,2,6),(1,6,5),(2,3,7),(2,7,6),(4,5,6),(4,6,7)]
	ff=[woo.dem.Facet.make((nn[i[0]],nn[i[1]],nn[i[2]]),**kw) for i in indices]
	if ff[0].shape.halfThick>0:
		for n in nn: woo.dem.DemData.setOriMassInertia(n)
	return ff




import woo.pyderived
class MeshImport(woo.core.Object,woo.pyderived.PyWooObject):
	'User interface for importing meshes from external files (STL), so that all import parameters are kept together.'
	_classTrait=None
	_PAT=woo.pyderived.PyAttrTrait
	_attrTraits=[
		_PAT(str,'file','',existingFilename=True,doc='Mesh file to be imported (STL)'),
		_PAT(float,'preScale',1.0,doc='Scaling, applied before other transformations.'),
		_PAT(woo.core.Node,'node',None,doc='Node defining local coordinate system for importing (already :obj:`prescaled <preScale>`) mesh; if not given, global coordinate system is assumed.'),
		_PAT(float,'halfThick',0,unit='mm',doc='Half thickness (:obj:`woo.dem.Facet.halfThick`) assigned to all created facets. If zero, don\'t assign :obj:`~woo.dem.Facet.halfThick`.'),
		_PAT(float,'tessMaxBox',0,unit='mm',doc='Some importers (STL) can tesselated triangles so that their bounding box dimension does not exceed :obj:`tessMaxBox`. If non-positive, no tesselation will be done.'),
	]
	def __init__(self,**kw):
		woo.core.Object.__init__(self)
		self.wooPyInit(self.__class__,woo.core.Object,**kw)
	def doImport(self,mat,mask=woo.dem.DemField.defaultBoundaryMask):
		fmt=None
		if self.file.lower().endswith('.stl'): fmt='stl'
		else: raise ValueError('Unknown mesh file extension (must be .stl).')
		if fmt=='stl':
			tri=woo.utils.importSTL(self.file,mat=mat,mask=mask,scale=self.preScale,shift=(self.node.pos if self.node else Vector3.Zero),ori=(self.node.ori if self.node else Quaternion.Identity),maxBox=self.tessMaxBox)
			return tri
		else: raise RuntimeError('Programming error: unhandled value fmt=="%s".'%(str(fmt)))

