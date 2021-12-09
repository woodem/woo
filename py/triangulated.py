from minieigen import *
import woo.pack, woo.utils
import numpy
import math

from woo._triangulated import *
log=woo.utils.makeLog(__name__)

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
    print('Ax is',ax)
    axOri=Quaternion([1,0,0,0]); axOri.setFromTwoVectors(Vector3.UnitX,ax)

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
    A,B,C,D=Vector3(A),Vector3(B),Vector3(C),Vector3(D)
    if sum(div)>0:
        if min(div)<1: raise ValueError('Both components of div must be positive')
        if size>0: raise ValueError('only one of *div* or *size* may be given (not both)')
    else:
        l1,l2=min((A-C).norm(),(D-B).norm()),min((A-B).norm(),(C-D).norm())
        if size!=0: div=Vector2i(int(max(2,math.ceil(l1/size))),int(max(2,math.ceil(l2/size))))
        else: div=Vector2i(1,1)
    AB,AC,CD,BD=B-A,C-A,D-C,D-B
    aabb,aacc=numpy.linspace(0,1,div[0]+1),numpy.linspace(0,1,div[1]+1)
    pts=[[A+ac*AC+ab*(B+ac*BD-(A+ac*AC)) for ac in aacc] for ab in aabb]
    return woo.pack.gtsSurface2Facets(woo.pack.sweptPolylines2gtsSurface(pts),**kw)

def box(dim,center,which=Vector6i(1,1,1,1,1,1),**kw):
    '''Return box created from facets. ``**kw`` args are passed to :obj:`woo.dem.Facet.make`. If facets have thickness, node masses and inertia will be computed automatically.

:param Vector3 dim: dimensions along x, y, z axes;
:param center: a :obj:`minieigen:Vector3` (box aligned with global axes) or :obj:`woo.core.Node` (local coordinate system) giving the center of the box;
:param Vector6i which: select walls which are to be included (all by default); the order is -x, -y, -z, +x, +y, +z. Thuus, e.g., to make box without top, say ``which=(1,1,1,1,1,0)``.
'''
    if not isinstance(center,woo.core.Node): c=woo.core.Node(pos=center)
    else: c=center
    nn=[woo.core.Node(pos=c.loc2glob(Vector3(.5*sgn[0]*dim[0],.5*sgn[1]*dim[1],.5*sgn[2]*dim[2]))) for sgn in ((-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1))]
    # vertex indices of faces of unit cube (two facets per face)
    indices=((0,2,1),(0,3,2),(0,1,5),(0,5,4),(0,4,3),(3,4,7),(1,2,6),(1,6,5),(2,3,7),(2,7,6),(4,5,6),(4,6,7))
    # mapping face to which (first two are -z, and so on)
    whichMap=(2,2,1,1,0,0, 3,3,4,4,5,5)
    ff=[woo.dem.Facet.make((nn[ii[0]],nn[ii[1]],nn[ii[2]]),**kw) for i,ii in enumerate(indices) if bool(which[whichMap[i]])]
    if ff[0].shape.halfThick>0:
        # it is harmless to run those on nodes which are unused due to `which`
        for n in nn: woo.dem.DemData.setOriMassInertia(n)
    return ff


def sweep2d(pts,zz,node=None,fakeVel=0.,halfThick=0.,shift=True,shorten=False,**kw):
    '''
    Build surface by perpendicularly sweeping list of 2d points along the 3rd axis (in global/local coordinates).

    :param pts: list of :obj:`minieigen:Vector2` containing :math:`xy` coordinates of points in local axes. If the point is ``(nan,nan)`` or ``None``, the line is split at this point. If the point contains ``(nan,i)``, then it refers to ``i``-th point which has been constructed up to now: positive number from the beginning of the line, negative from the end.
    :param zz: tuple or :obj:`minieigen:Vector2` containing ``(z0,z1)`` coordinates for all points.
    :param node: optional instance of :obj:`woo.core.Node` defining local coordinate system; if not given, global coordinates are used.
    :param float fakeVel: assign :obj:`~woo.dem.Facet.fakeVel` to facets, in the sweeping direction at every face
    :param halfThick: if given and *halfThick* is ``True``, shift points so that *pts* give points of the outer surface (and nodes are shifted inwards); in this case, the ordering of pts is important.
    :param shift: activate point shifting, in conjunction with *halfThick*;
    :param shorten: move endpoints away from the endpoint (along the segment) by *halfThick*;
    '''
    from math import isnan
    nan=float('nan')
    nn=[] # list of nodes
    ff=[] # list of facets
    stacks=len(zz)
    if stacks<2: raise ValueError(f'len(zz)≡stacks must be greater than 1 (not {stacks})')

    # tell whether pp[i] is an invalid point (beginning, ending, nan, None)
    def invalid(pp,i): return i<0 or i>=len(pp) or pp[i] is None or math.isnan(pp[i].maxAbsCoeff())
    # same as invalid, but point with a single NaN is a point (referring another node)
    def notPt(pp,i): return i<0 or i>=len(pp) or pp[i] is None or (isnan(pp[i][0]) and isnan(pp[i][1]))
    # reference point: NaN and some number
    def refPt(pp,i): return i>=0 and i<len(pp) and isnan(pp[i][0]) and not isnan(pp[i][1])
    # return nodes referred from point
    def refNodes(pp,i,negRefAdd):
        assert refPt(pp,i)
        j=int(pp[i][1])
        if j<0: j+=negRefAdd
        return nn[stacks*j:stacks*(j+1)]

    def angleBetween(a,b):
        # http://stackoverflow.com/a/2663678/761090
        a1,a2=math.atan2(a[1],a[0]),math.atan2(b[1],b[0])
        if a2>a1+math.pi: a2-=2*math.pi
        elif a2<a1-math.pi: a2-=2*math.pi
        return a2-a1
    pts=[Vector2(p) if p is not None else Vector2(nan,nan) for p in pts] # fix types, so that all vector methods work

    if shift and halfThick!=0:
        pts2=[]
        def perp(v): return Vector2(-v[1],v[0]).normalized()
        for i in range(0,len(pts)):
            if invalid(pts,i):
                pts2.append(pts[i]) # copy the same invalid on
                continue
            # find average perpendicular vector at that point
            begPt,endPt=invalid(pts,i-1),invalid(pts,i+1)
            if begPt and endPt:
                pts2.append(pts[i])
                continue # might be reffering elsewhere... raise ValueError("Point at %d is not connected to any other."%i)
            if begPt: pts2.append(pts[i]-perp(pts[i+1]-pts[i])*halfThick+((pts[i+1]-pts[i]).normalized()*halfThick if shorten else Vector2(0,0)))
            elif endPt: pts2.append(pts[i]-perp(pts[i]-pts[i-1])*halfThick+((pts[i-1]-pts[i]).normalized()*halfThick if shorten else Vector2(0,0)))
            else:
                e1,e2=(pts[i-1]-pts[i]).normalized(),(pts[i+1]-pts[i]).normalized()
                a=angleBetween(e1,e2)
                d=(halfThick/math.sin(a/2.))*(e1+e2).normalized()
                pts2.append(pts[i]+d)
    else: pts2=pts
    # if len(zz)!=2: raise ValueError('len(zz) must be 2 (not %d)'%len(zz))
    cs=(node if node else woo.core.Node(pos=(0,0,0),ori=Quaternion.Identity))
    for i in range(0,len(pts2)):
        if notPt(pts2,i): continue
        begPt,endPt=notPt(pts2,i-1),notPt(pts2,i+1)
        #print i,begPt,endPt
        if refPt(pts2,i):
            left=refNodes(pts2,i,negRefAdd=0)
            negRefAdd=0
        else:
            nn+=[woo.core.Node(pos=cs.loc2glob((pts2[i][0],pts2[i][1],z)),dem=woo.dem.DemData(blocked='xyzXYZ')) for z in zz]
            left=nn[-stacks:]
            # C,D=nn[-2],nn[-1]
            # this added two nodes, so references counting backwards must go one further
            negRefAdd=-1
        # beginning point does nothing else, all other add the last segment
        if not begPt:
            if refPt(pts2,i-1):
                right=refNodes(pts,i-1,negRefAdd=negRefAdd)
            else: right=nn[-2*stacks:-stacks]
            assert len(nn)>=4 # there must be at least 4 points already
            # fakeVel
            if fakeVel==0.: fv=Vector3.Zero
            else: fv=(right[0].pos-left[0].pos).normalized()*fakeVel
            for i in range(stacks-1):
                for verts in [(left[i],right[i],right[i+1]),(left[i],right[i+1],left[i+1])]:
                    ff.append(woo.dem.Facet.make(verts,halfThick=halfThick,fakeVel=fv,**kw))
            #ff+=[
            #    woo.dem.Facet.make((A,C,D),halfThick=halfThick,fakeVel=fv,**kw),
            #    woo.dem.Facet.make((A,D,B),halfThick=halfThick,fakeVel=fv,**kw)
            #]
    return ff



import woo.pyderived
class MeshImport(woo.core.Object,woo.pyderived.PyWooObject):
    '''User interface for importing meshes from external files , so that all import parameters are kept together. Currently supported formats are:

    * STL: both ascii and binary formats; :obj:`tagged` not supported (meaningless)
    * nastran: subset, ``GRID`` and ``CTRIA3`` tags are recognized, others ignored; :obj:`tagged` is supported, nodal numbers are preserved.
    
    '''
    _classTrait=None
    _PAT=woo.pyderived.PyAttrTrait
    _attrTraits=[
        _PAT(str,'file','',existingFilename=True,doc='Mesh file to be imported (STL)'),
        _PAT(float,'preScale',1.0,doc='Scaling, applied before other transformations.'),
        _PAT(woo.core.Node,'node',None,doc='Node defining local coordinate system for importing (already :obj:`prescaled <preScale>`) mesh; if not given, global coordinate system is assumed.'),
        _PAT(float,'halfThick',0,unit='mm',doc='Half thickness (:obj:`woo.dem.Facet.halfThick`) assigned to all created facets. If zero, don\'t assign :obj:`~woo.dem.Facet.halfThick`.'),
        _PAT(Vector3,'fakeVel',Vector3.Zero,unit='m/s',doc=':obj:`Fake surface velocity <woo.dem.Facet.fakeVel>` set (as-is) on all imported facets; if ``(0,0,0)`` (default), nothing is set.'),
        _PAT(float,'tessMaxBox',0,unit='mm',doc='Some importers (STL) can tesselated triangles so that their bounding box dimension does not exceed :obj:`tessMaxBox`. If non-positive, no tesselation will be done.'),
        _PAT(bool,'tagged',False,doc='Use :obj:`woo.dem.DemDataTagged` to keep information about node number at input; this is only supported for some formats, and exception will be raise if used with format not supporting it }e.g. STL does not store vertices)'),
        _PAT(float,'color',float('nan'),'Pass resulting particle color (scalar) when importing; only effective for formats which support it (STL). *nan* disables'),
    ]
    def __new__(klass,**kw):
        self=super().__new__(klass)
        self.wooPyInit(MeshImport,woo.core.Object,**kw)
        return self
    def __init__(self,**kw):
        woo.core.Object.__init__(self)
        self.wooPyInit(MeshImport,woo.core.Object,**kw)
    def doImport(self,mat,mask=woo.dem.DemField.defaultStaticMask,**kw):
        'Do the actual import. Nastern: ``**kw`` is passed to :obj:`woo.dem.Facet.make`. STL: some values from ``**kw`` used (``color``) and passed to :obj:`woo.utils.importSTL <woo._utils2.importSTL>`.'
        fmt=None
        if self.file.lower().endswith('.stl'): fmt='stl'
        elif self.file.lower().endswith('.nas'): fmt='nastran'
        else: raise ValueError('Unknown mesh file extension (must be .stl, .nas).')
        if 'thickness' in kw: raise ValueError('Use MeshImport.halfThick attribute to set thickness, instead of passing MeshImport.doImport(thickness=...).')

        # called after import for each format, before returning
        def applyFakeVel(tri):
            if self.fakeVel!=Vector3.Zero:
                for t in tri: t.shape.fakeVel=self.fakeVel

        if fmt=='stl':
            if self.tagged: raise ValueError('MeshImport.tagged: not supported (meaningless) with STL format.')
            kw2={}
            # extract recognized values from **kw
            for ex in ['color']:
                if ex in kw: kw2[ex]=kw[ex]
            if not math.isnan(self.color):
                log.warn('Using user-defined color %g for STL import'%self.color)
                kw2['color']=self.color
            tri=woo.utils.importSTL(self.file,mat=mat,mask=mask,scale=self.preScale,shift=(self.node.pos if self.node else Vector3.Zero),ori=(self.node.ori if self.node else Quaternion.Identity),maxBox=self.tessMaxBox,thickness=2*self.halfThick,**kw2)
            applyFakeVel(tri)
            return tri
        elif fmt=='nastran':
            nodeMap={} # mapping of nodes numbers (tags) to Node objects
            tri=[]
            with open(self.file,'r') as infile:
                for lineno,l in enumerate(infile):
                    l=l[:-1] # chomp newline
                    if '$' in l: l=l[:l.index('$')] # discard comments
                    if l.startswith('GRID '):
                        try:
                            # if l[45]=='-': l=l[:45]+'   '
                            tag,x,y,z=int(l[8:16]),self.preScale*float(l[24:32]),self.preScale*float(l[32:40]),self.preScale*float(l[40:48])
                        except:
                            log.error('%s:%d: Error reading line'%(self.file,lineno+1))
                            raise
                        dta=woo.dem.DemDataTagged(tag=tag) if self.tagged else woo.dem.DemData()
                        nodeMap[tag]=woo.core.Node(dem=dta,pos=(self.node.loc2glob((x,y,z)) if self.node else (x,y,z)))
                    elif l.startswith('CTRIA3 '):
                        a,b,c=int(l[24:32]),int(l[32:40]),int(l[40:48])
                        tri.append(woo.dem.Facet.make([nodeMap[a],nodeMap[b],nodeMap[c]],mat=mat,halfThick=self.halfThick,**kw))
                    # lines we know to ignore
                    elif l.startswith('BEGIN BULK') or l.startswith('PSHELL') or l.startswith('MAT') or l.startswith('ENDDATA') or len(l.strip())==0: continue
                    else: log.error('%s:%d: unparsed line skipped: %s'%self.file,lineno+1,l)
            applyFakeVel(tri)
            return tri
        else: raise RuntimeError('Programming error: unhandled value fmt=="%s".'%(str(fmt)))

