# encoding: utf-8
#
# utility functions for woo
#
# 2008-2009 © Václav Šmilauer <eudoxos@arcig.cz>

"""Heap of functions that don't (yet) fit anywhere else.
"""
import math,random,doctest
import woo, woo.dem, woo.core
import sys,os,warnings
from minieigen import *

from woo.dem import *
from woo.fem import *
from woo.core import *
import woo
import woo.config

# c++ implementations for performance reasons
#from woo._utils import *
#from woo._utils2 import *
from woo._utils2 import *


# declare that _utils2 should be documented here
_docInlineModules=(woo._utils2,)

inf=float('inf')

# try at import time, this is expensive
hasGL=True
try: import woo.gl
except ImportError: hasGL=False


def find_executable(ex):
    'Thin wrapper for find_executable, using either shutil.which or distutils.spawn.find_executable.'
    try:
        import shutil
        return shutil.which(ex)
    except ImportError:
        import distutils.spawn
        return distutils.spawn.find_executable(ex)

# https://stackoverflow.com/a/55276759/761090
import logging
from functools import partial,partialmethod
logging.TRACE=logging.DEBUG-5
logging.addLevelName(logging.TRACE,'TRACE')
logging.Logger.trace=partialmethod(logging.Logger.log,logging.TRACE)
logging.trace=partial(logging.log,logging.TRACE)

def makeLog(name):
    import logging
    try:
        import coloredlogs
        coloredlogs.install(level='INFO',fmt='%(asctime)s %(name)-8s %(filename)s:%(lineno)d [%(levelname)s] %(message)s',datefmt='%H:%M:%S')
        return logging.getLogger(name)
    except ImportError: pass
    ## does not really color... why?
    #try:
    #    import colorlog
    #    handler=colorlog.StreamHandler()
    #    handler.setFormatter(colorlog.ColoredFormatter('%(asctime)s %(name)-8s %(filename)s:%(lineno)d [%(levelname)s] %(message)s',datefmt='%H:%M:%S',reset=True))
    #    ret=colorlog.getLogger(name)
    #    ret.addHandler(handler)
    #    ret.setLevel('INFO')
    #    return ret
    #except ImportError: pass
    return logging.getLogger(name)

# use right away, for functions in this module
log=makeLog(__name__)


def spherePWaveDt(radius,density,young):
    r"""Compute P-wave critical timestep for a single (presumably representative) sphere, using formula for P-Wave propagation speed :math:`\Delta t_{c}=\frac{r}{\sqrt{E/\rho}}`. If you want to compute minimum critical timestep for all spheres in the simulation, use :obj:`woo.utils.pWaveDt` instead.

    >>> spherePWaveDt(1e-3,2400,30e9)
    2.8284271247461903e-07

    """
    from math import sqrt
    return radius/sqrt(young/density)

def defaultMaterial():
    """Return default material, when creating particles with :obj:`woo.utils.sphere` and friends and material is unspecified. This function returns ``FrictMat(density=1e3,young=1e7,ktDivKn=.2,tanPhi=tan(.5))``.

    """
    import math
    return FrictMat(density=1e3,young=1e7,ktDivKn=.2,tanPhi=math.tan(.5))

def defaultEngines(damping=0.,gravity=None,verletDist=-.05,kinSplit=False,dontCollect=False,noSlip=False,noBreak=False,cp2=None,law=None,model=None,collider='sap',grid=None,dynDtPeriod=100,cpKw={},lawKw={}):
    """Return default set of engines, suitable for basic simulations during testing."""
    if (grid is not None) and grid:
        log.warning('*grid* argument is deprecated, use collider="grid" instead.')
        collider='grid'
    if gravity: raise ValueError("gravity MUST NOT be specified anymore, set DemField.gravity=... instead.")
    if model:
        if cp2 or law: warnings.warn("cp2 and law args are ignored when model is provided.")
        if damping!=0.: warnings.warn("damping is ignored when model is provided.")
        cp2,law=model.getFunctors()
        if len(law)==1: law[0].label='contactLaw'
        if len(cp2)==1: cp2[0].label='cp2'
        damping=model.getNonviscDamping()
        if model.distFactor!=1.0:
            woo.master.checkApi(10103,"woo.models.ContactModelSelector.distFactor has non-default value. The new API does not set this value in engines returned from DemField.minimalEngines anymore, it *must* be set as DemField.distFactor by the caller.")
    else:
        cp2=[cp2 if cp2 else Cp2_FrictMat_FrictPhys()]
        law=[law if law else Law2_L6Geom_FrictPhys_IdealElPl(noSlip=noSlip,noBreak=noBreak,label='contactLaw')]
        
    cp2[0].updateAttrs(cpKw)
    law[0].updateAttrs(lawKw)

    bo1_aabb=[Bo1_Sphere_Aabb(),Bo1_Facet_Aabb(),Bo1_Wall_Aabb(),Bo1_InfCylinder_Aabb(),Bo1_Ellipsoid_Aabb(),Bo1_Rod_Aabb(),Bo1_Capsule_Aabb(),Bo1_Cone_Aabb()]

    if collider=='sap': coll=InsertionSortCollider(bo1_aabb,label='collider',verletDist=verletDist)
    elif collider=='tree': coll=AabbTreeCollider(bo1_aabb,label='collider',verletDist=verletDist)
    elif collider=='grid': coll=GridCollider([Grid1_Sphere(),Grid1_Facet(),Grid1_Wall(),Grid1_InfCylinder()],label='collider',verletDist=verletDist)

    return [
        Leapfrog(damping=damping,reset=True,kinSplit=kinSplit,dontCollect=dontCollect,label='leapfrog'),
        coll,
        ContactLoop(
            [Cg2_Sphere_Sphere_L6Geom(),Cg2_Facet_Sphere_L6Geom(),Cg2_Wall_Sphere_L6Geom(),Cg2_InfCylinder_Sphere_L6Geom(),Cg2_Ellipsoid_Ellipsoid_L6Geom(),Cg2_Sphere_Ellipsoid_L6Geom(),Cg2_Wall_Ellipsoid_L6Geom(),Cg2_Wall_Facet_L6Geom(),Cg2_Rod_Sphere_L6Geom(),Cg2_Wall_Capsule_L6Geom(),Cg2_Capsule_Capsule_L6Geom(),Cg2_InfCylinder_Capsule_L6Geom(),Cg2_Facet_Capsule_L6Geom(),Cg2_Sphere_Capsule_L6Geom(),Cg2_Facet_Facet_L6Geom(),Cg2_Facet_InfCylinder_L6Geom(),Cg2_Cone_Sphere_L6Geom()],
            cp2,law,applyForces=True,label='contactLoop'
        ),
    ]+([woo.dem.DynDt(stepPeriod=dynDtPeriod,label='dynDt')] if dynDtPeriod>0 else [])

def _commonBodySetup(b,nodes,mat,fixed=False):
    """Assign common body parameters."""
    if isinstance(mat,Material): b.mat=mat
    elif callable(mat): b.mat=mat()
    else: raise TypeError("The 'mat' argument must be Material instance, or a callable returning Material.");
    b.shape.nodes=nodes
    if len(nodes)==1:
        b.updateMassInertia()
    else:
        for n in b.shape.nodes:
            n.dem.mass=0; n.dem.inertia=Vector3.Zero
    for i,n in enumerate(b.nodes):
        n.dem.addParRef(b) # tell the node that it has that particle
        if fixed==None: pass # do not modify blocked at all
        elif fixed==True: n.dem.blocked='xyzXYZ'
        else: n.dem.blocked=''.join(['XYZ'[ax] for ax in (0,1,2) if n.dem.inertia[ax]==inf]) # block rotational DOFs where inertia is infinite

def _mkDemNode(**kw):
    '''Helper function to create new Node instance, with dem set to an new empty instance.'''
    if hasGL: return Node(dem=DemData(),gl=woo.gl.GlData(),**kw)
    else: return Node(dem=DemData(),**kw)

def sphere(center,radius,mat=defaultMaterial,fixed=False,wire=False,color=None,highlight=False,mask=DemField.defaultMovableMask,vel=None):
    """Create sphere with given parameters; mass and inertia computed automatically.

    :param Vector3 center: center
    :param float radius: radius
    :param float-or-None: particle's color as float; random color will be assigned if ``None``.
    :param mat:
        specify :obj:`woo.dem.Particle.material`; different types are accepted:
            * :obj:`woo.dem.Material` instance: this instance will be used
            * callable: will be called without arguments; returned Material value will be used (Material factory object, if you like)
    :param int mask: :obj:`woo.dem.Particle.mask` for the body

    :return:
        Particle instance with desired characteristics.

    Instance of material can be given::

        >>> from woo import utils
        >>> s1=utils.sphere((0,0,0),1,wire=False,color=.7,mat=ElastMat(young=30e9,density=2e3))
        >>> s1.shape.wire
        False
        >>> s1.shape.color
        0.7
        >>> s1.mat.density
        2000.0

    Finally, material can be a callable object (taking no arguments), which returns a Material instance.
    Use this if you don't call this function directly (for instance, through woo.pack.randomDensePack), passing
    only 1 *material* parameter, but you don't want material to be shared.

    For instance, randomized material properties can be created like this:

        >>> import random
        >>> def matFactory(): return ElastMat(young=1e10*random.random(),density=1e3+1e3*random.random())
        ...
        >>> s2=utils.sphere([0,2,0],1,mat=matFactory)
        >>> s3=utils.sphere([1,2,0],1,mat=matFactory)

    """
    b=Particle()
    b.shape=Sphere(radius=radius,color=color if color else random.random())
    b.shape.wire=wire
    _commonBodySetup(b,([center] if isinstance(center,Node) else [_mkDemNode(pos=center),]),mat=mat,fixed=fixed)
    if vel: b.vel=vel
    #b.aspherical=False
    b.mask=mask
    return b

def ellipsoid(center,semiAxes,ori=Quaternion.Identity,angVel=None,color=None,mat=defaultMaterial,fixed=False,wire=False,visible=True,mask=DemField.defaultMovableMask,**kw):
    'Return an :obj:`woo.dem.Ellipsoid` particle.'
    p=Particle()
    p.shape=Ellipsoid(semiAxes=semiAxes,color=color if color else random.random())
    _commonBodySetup(p,([center] if isinstance(center,Node) else [_mkDemNode(pos=center,ori=ori),]),mat=mat,fixed=fixed)
    p.shape.wire=wire
    #p.aspherical=True
    p.mask=mask
    p.shape.visible=visible
    if angVel: p.shape.nodes[0].dem.angVel=angVel
    return p

def capsule(center,radius,shaft,ori=Quaternion.Identity,fixed=False,color=None,wire=False,mat=defaultMaterial,mask=DemField.defaultMovableMask):
    'Return a ready-made capsule particle.'
    p=Particle()
    p.shape=Capsule(radius=radius,shaft=shaft,color=color if color else random.random())
    _commonBodySetup(p,([center] if isinstance(center,Node) else [_mkDemNode(pos=center,ori=ori),]),mat=mat,fixed=fixed)
    p.shape.wire=wire
    #p.aspherical=True
    p.mask=mask
    return p

def cone(vertices,radii,*,fixed=True,wire=False,color=None,angVel=None,mat=defaultMaterial,visible=True,mask=DemField.defaultBoundaryMask):
    '''
    Create cone with given parameters.
    '''
    assert len(vertices)==2
    nodes=[]
    for i in (0,1):
        if isinstance(vertices[i],Node): nodes.append(vertices[i])
        else: nodes.append(_mkDemNode(pos=vertices[i]))
        if not nodes[-1].dem: nodes[-1].dem=DemData()
    p=Particle()
    p.shape=Cone(radii=radii,color=color if color else random.random())
    p.shape.wire=wire
    _commonBodySetup(p,nodes,mat=mat,fixed=fixed)
    p.mask=mask
    p.shape.visible=visible
    if angVel is not None:
        if isinstance(angVel,(float,int)): angVel=(p.nodes[1].pos-p.nodes[0].pos).normalized()*angVel
        p.nodes[0].dem.angVel=angVel
    return p


def wall(position,axis,sense=0,glAB=None,fixed=True,mass=0,color=None,mat=defaultMaterial,visible=True,mask=DemField.defaultBoundaryMask):
    """Return ready-made wall body.

    :param float-or-Vector3-or-Node position: center of the wall. If float, it is the position along given axis, the other 2 components being zero
    :param ∈{0,1,2} axis: orientation of the wall normal (0,1,2) for x,y,z (sc. planes yz, xz, xy)
    :param ∈{-1,0,1} sense: sense in which to interact (0: both, -1: negative, +1: positive; see :obj:`woo.dem.Wall`)

    See :obj:`woo.utils.sphere`'s documentation for meaning of other parameters."""
    p=Particle()
    p.shape=Wall(sense=sense,axis=axis,color=color if color else random.random())
    if glAB: p.shape.glAB=glAB
    if not fixed and mass<=0: raise ValueError("Non-fixed wall must have positive mass")
    if isinstance(position,(int,float)):
        pos2=Vector3(0,0,0); pos2[axis]=position
        node=_mkDemNode(pos=pos2)
    elif isinstance(position,Node): node=position
    else: node=_mkDemNode(pos=position)
    _commonBodySetup(p,[node],mat=mat,fixed=fixed)
    p.shape.nodes[0].dem.mass=mass
    #p.aspherical=False # wall never rotates anyway
    p.mask=mask
    p.shape.visible=visible
    return p

def wallBox(box,which=(1,1,1,1,1,1),**kw):
    '''Return box delimited by walls, created by :obj:`woo.dem.Wall.make`, which receives most arguments. :obj:`Wall.glAB <woo.dem.Wall.glAB>` are computed automatically so that walls visually end at the edges.
    
    .. note:: Since :obj:`walls <woo.dem.Wall>` are infinite, they will still interact with other particle beyond this box; use :obj:`woo.triangulated.box` for true box with arbitrary orientation.
    
    :param box: axis-aligned box determining positions of walls.
    :param which: determines which of the 6 walls are created (boolean values), in the order -x, -y, -z, +x, +y, +z. For instance, to create a box which does not have the top, say ``which=(1,1,1,1,1,0)``).
    :returns: list of :obj:`~woo.dem.Particle` objects.
    '''
    ret=[]
    box=AlignedBox3(box) # force conversion
    if len(which)!=6: raise ValueError("Wall.makeBox: which must be a sequence of 6 boolean-convertibles.")
    for sense,ix,coord in [(1,0,box.min),(-1,1,box.max)]:
        for ax in (0,1,2):
            if not which[3*ix+ax]: continue
            ax1,ax2=(ax+1)%3,(ax+2)%3
            ret.append(woo.dem.Wall.make(coord[ax],sense=sense,axis=ax,glAB=AlignedBox2((box.min[ax1],box.min[ax2]),(box.max[ax1],box.max[ax2])),**kw))
    return ret

def rod(vertices,radius,fixed=True,wire=True,color=None,mat=defaultMaterial,visible=True,mask=DemField.defaultBoundaryMask,__class=woo.dem.Rod):
    '''Create :obj:`~woo.dem.Rod` with given parameters:

        :param vertices: endpoints given as coordinates (Vector3) or nodes
        :param radius: radius of the rod
        :param wire: render as wire by default
        :param color: color as scalar (0...1); if not given, random color is assigned
        :param mat: material; if not given, default material is assigned
        :param visible:
        :param mask:
    '''
    assert len(vertices)==2
    nodes=[]
    for i in (0,1):
        if isinstance(vertices[i],Node): nodes.append(vertices[i])
        else: nodes.append(_mkDemNode(pos=vertices[i]))
        if not nodes[-1].dem: nodes[-1].dem=DemData()
    p=Particle()
    p.shape=__class(radius=radius,color=color if color else random.random())
    p.shape.wire=wire
    _commonBodySetup(p,nodes,mat=mat,fixed=fixed)
    p.mask=mask
    p.shape.visible=visible
    return p

def truss(*args,**kw):
    return rod(__class=woo.dem.Truss,*args,**kw)



def facet(vertices,fakeVel=None,halfThick=0.,fixed=True,wire=True,color=None,highlight=False,mat=defaultMaterial,visible=True,mask=DemField.defaultBoundaryMask,flex=None,__class=woo.dem.Facet):
    """Create facet with given parameters.

    :param [Vector3,Vector3,Vector3] vertices: coordinates of vertices in the global coordinate system.
    :param bool wire: if ``True``, facets are shown as skeleton; otherwise facets are filled

    See :obj:`woo.utils.sphere`'s documentation for meaning of other parameters."""
    p=Particle()
    nodes=[]
    if isinstance(vertices[0],Node):
        nodes=vertices
        for n in nodes:
            if not n.dem: n.dem=DemData()
    else:
        nodes=[_mkDemNode(pos=vertices[0]),_mkDemNode(pos=vertices[1]),_mkDemNode(pos=vertices[2])]
    if flex!=None:
        if flex:
            warnings.warn('Facet.make(...,flex=True) is deprecated, use Membrane.make(...) instead.',DeprecationWarning,stacklevel=2)
            __class=woo.fem.Membrane
    p.shape=__class(color=color if color else random.random(),halfThick=halfThick)
    if fakeVel: p.shape.fakeVel=fakeVel
    p.shape.wire=wire
    _commonBodySetup(p,nodes,mat=mat,fixed=fixed)
    #p.aspherical=False # mass and inertia are 0 anyway; fell free to change to ``True`` if needed
    p.mask=mask
    p.shape.visible=visible
    return p

def membrane(*args,**kw):
    if 'flex' in kw: raise ValueError("Membrane.make: *flex* argument not accepted (use Membrane.make for Membranes and Facet.make for pure facets).")
    return facet(__class=woo.fem.Membrane,*args,**kw)

def tetra(vertices,fixed=True,wire=True,color=None,highlight=False,mat=defaultMaterial,visible=True,mask=DemField.defaultBoundaryMask,__class=woo.fem.Tetra):
    '''Create tetrahedral particle'''
    p=Particle()
    nodes=[]
    for i in range(4):
        if isinstance(vertices[i],Node):
            nodes.append(vertices[i])
            if not nodes[-1].dem: nodes[-1].dem=DemData()
        else: nodes.append(_mkDemNode(pos=vertices[i]))
    p.shape=__class(color=color if color else random.random())
    assert len(nodes)==4
    _commonBodySetup(p,nodes,mat=mat,fixed=fixed)
    #p.aspherical=False
    p.mask=mask
    p.shape.wire=wire
    p.shape.visible=visible
    p.shape.canonicalizeVertexOrder()
    return p

def tet4(*args,**kw):
    return tetra(__class=woo.fem.Tet4,*args,**kw)

def infCylinder(position,radius,axis,glAB=None,fixed=True,mass=0,color=None,wire=False,angVel=None,mat=defaultMaterial,mask=DemField.defaultMovableMask):
    """Return a ready-made infinite cylinder particle."""
    p=Particle()
    p.shape=InfCylinder(radius=radius,axis=axis,color=color if color else random.random())
    if glAB: p.shape.glAB=glAB
    p.shape.wire=wire
    node=position if isinstance(position,Node) else _mkDemNode(pos=position)
    _commonBodySetup(p,[node],mat=mat,fixed=fixed)
    #p.aspherical=False # only rotates along one axis
    p.mask=mask
    if angVel is not None: p.angVel=angVel
    return p


def importNmesh(filename,mat,mask,trsf=None,dem=None,surf=True,surfMat=None,surfMask=None,surfHalfThick=0.):
    '''
    Import nmesh file, text format written by `Netgen <http://sourceforge.net/projects/netgen-mesher/>`__. All nodes are created with :obj:`woo.dem.DemData.blocked` equal to ``XYZ`` (no rotations, but all translations allowed); if some of them are to be fixed, this must be done by the user after calling the function. Tetrahedra mass is lumped onto nodes. Thickness of surface triangles (if any) is ignored for mass lumping.

    :param mat: material for volume (:obj:`woo.fem.Tet4`) elements.
    :param mask: :obj:`woo.dem.Particle.mask` for new volume elements.
    :param trsf: callable for transforming input-file vertex coordinate to the simulation space.
    :param dem: a :obj:`woo.dem.DemField` instance; if given, nodes and particles will be added to this :obj:`~woo.dem.DemField`.
    :param surf: create surface triangulation, for collisions.
    :param surfMat: material for surface (:obj:`woo.dem.Facet`); if ``None``, *mat* is used.
    :param surfMask: mask for surface; if ``None``, *mask* is used.
    :param surfHalfThick: :obj:`woo.dem.Facet.halfThick` for the surface (must be positive for collisions between two triangulated surfaces).
    :return: ([Node,Node,...],[Particle,Particle,...])
    '''
    with open(filename) as m:
        mesh=[]
        lens=[]
        for ll in m:
            l=ll[:-1].split()
            if len(l)==1:
                mesh.append([])
                lens.append(int(l[0])) # for consistency check below
            elif len(mesh)==1:
                v=Vector3(tuple([float(i) for i in l]))
                if trsf: v=trsf(v)
                mesh[-1].append(v)
            else: mesh[-1].append(tuple([int(i)-1 for i in l[1:]]))
    for i,name in [(0,'nodes'),(1,'triangles'),(2,'tetrahedra')]:
        if lens[i]!=len(mesh[i]): raise RuntimeError('Inconsistent nmesh file %s: number of %s should be %d, is %d.'%(filename,name,lens[i]),len(mesh[i]))
    if len(mesh)!=3: raise RuntimeError('Error in %s: incorrect number of section (%d, should be 3)'%(filename, len(mesh)))
    if surfMat==None: surfMat=mat
    if surfMask==None: surfMask=mask
    nn=[Node(pos=p,dem=DemData(blocked='XYZ')) for p in mesh[0]]
    t4=[Tet4.make((nn[t[0]],nn[t[1]],nn[t[2]],nn[t[3]]),mat=mat,wire=True,fixed=None,mask=mask) for t in mesh[1]]
    if surf: f3=[Facet.make((nn[t[0]],nn[t[1]],nn[t[2]]),mat=mat,wire=True,halfThick=surfHalfThick,fixed=None,mask=surfMask) for t in mesh[2]]
    else: f3=[]
    # lump mass
    for n in nn:
        n.dem.mass=sum([p.material.density*.25*p.shape.getVolume() for p in n.dem.parRef if isinstance(p.shape,Tet4)])
    if dem:
        dem.nodesAppend(nn)
        dem.par.add(t4+f3)
    return nn,t4+f3


def ensureDir(d,msg='Creating directory %s'):
    'Ensure that directory ``d`` exists. If ``d`` is empty, return immediately (signifies usually the current directory). Otherwise try to create the directory. *msg* is an optional message to be printed to stdout only when something is actually done, and must include %s which will be replaced by the directory.'
    if d=='': return # current dir
    # this checks only if something will be created; may not see it in some corner cases... doesn't matter, just diagnostics
    if not os.path.exists(d) and msg: print(msg%d)
    os.makedirs(d,exist_ok=True)


def makeVideo(frameSpec,out,renameNotOverwrite=True,fps=24,kbps=15000,holdLast=-.5,open=False):
    """Create a video from external image files using `mencoder <http://www.mplayerhq.hu>`__, `avconv <http://libav.org/avconv.html>`__ or `ffmpeg <http://www.ffmpeg.org>`__ (whichever is found on the system). Encodes using the default mencoder codec (mpeg4), two-pass with mencoder and one-pass with avconv, running multi-threaded with number of threads equal to number of OpenMP threads allocated for Woo.

    Some *out* extensions (``.gif``, ``.mng``) will call ImageMagick (``convert``; likely not available under Windows) to create animated image instead of video file.

    :param frameSpec: wildcard | sequence of filenames. If list or tuple, filenames to be encoded in given order; otherwise wildcard understood by mencoder's mf:// URI option (shell wildcards such as ``/tmp/snap-*.png`` or and printf-style pattern like ``/tmp/snap-%05d.png``), if using mencoder, or understood by ``avconv`` or ``ffmpeg`` if those are being used.
    :param str out: file to save video into
    :param bool renameNotOverwrite: if True, existing same-named video file will have -*number* appended; will be overwritten otherwise.
    :param int fps: Frames per second (``-mf fps=…``)
    :param int kbps: Bitrate (``-lavcopts vbitrate=…``) in kb/s (ignored for animated images)
    :param holdLast: Repeat the last frame this many times; if negative, it means seconds and will be converted to frames according to *fps*. This option is not applicable if *frameSpec* is a wildcard (as opposed to a list of images).
    :param open: Open the resulting movie file with the default system program for that type of file.

    .. note:: ``avconv`` and ``ffmpeg`` need symlinks, which are not available under Windows.
    """
    # find which encoder to actually use
    menc=find_executable('mencoder')
    ffmp=find_executable('ffmpeg')
    avco=find_executable('avconv')
    conv=find_executable('convert')
    # the first one wins
    WIN=(sys.platform=='win32')
    IMG=out.endswith('.gif') or out.endswith('.mng')
    if not IMG:
        if avco and not WIN: encExec,encType=avco,'avconv'
        elif ffmp and not WIN: encExec,encType=ffmp,'avconv'
        elif menc: encExec,encType=menc,'mencoder'
        else: raise RuntimeError('No suitable video encoder (ffmpeg (non-Windows), avconv (non-Windows), mencoder) found in PATH.')
    else:
        if not conv: raise RuntimeError('Convert (from ImageMagick) not found for animated image conversion.')
        encExec,encType=conv,'IMG'

    import os,os.path,subprocess
    if renameNotOverwrite and os.path.exists(out):
        i=0
        while(os.path.exists(out+"~%d"%i)): i+=1
        os.rename(out,out+"~%d"%i); log.warning("Output file `%s' already existed, old file renamed to `%s'"%(out,out+"~%d"%i))
    if holdLast<0: holdLast*=-fps
    if isinstance(frameSpec,list) or isinstance(frameSpec,tuple):
        if holdLast>0: frameSpec=list(frameSpec)+int(holdLast)*[frameSpec[-1]]
        frameSpecMenc=','.join(frameSpec)
        frameSpecAvconv=list(frameSpec)
    else:
        frameSpecMenc=frameSpec
        frameSpecAvconv=[frameSpec]
        if encType=='avconv': raise RuntimeError('Encoding via ffmpeg/avconv is only possible when list of files (as opposed to a pattern) is passed.')

    if encType=='avconv' or encType=='IMG':
        if sys.platform=='win32': raise RuntimeError('Encoding via ffmpeg/avconv and to animated images is not yet implemented under Windows (symlinks not functional on this platform).')
        symDir=woo.master.tmpFilename()
        os.mkdir(symDir)
        ff2=[]
        ext=os.path.splitext(frameSpecAvconv[0])[1]
        symPattern=symDir+'/%05d'+ext
        for i,f in enumerate(frameSpecAvconv):
            ff2.append(symPattern%i)
            os.symlink(os.path.abspath(f),ff2[-1])
        frameSpecAvconv=ff2

    if IMG:
        cmd=[conv,'-delay',str(int(.5+(1./fps)/100.)),'-loop','0']+frameSpecAvconv+[out]
        log.info('Animated image: ',' '.join(cmd))
        ret=subprocess.call(cmd)
        if ret!=0: raise RuntimeError("Error running %s."%encExec)

    else:
        devNull='nul' if (sys.platform=='win32') else '/dev/null'
        # mencoder normally creates divx2pass.log in the current dir, which might fail
        # use temp file instead, explicitly
        passLogFile=woo.master.tmpFilename()+'.log'
        # passNo==0 means single pass (avconv)
        for passNo in ((1,2) if encType=='mencoder' else (0,)):
            if encType=='mencoder': cmd=[encExec,'mf://%s'%frameSpecMenc,'-mf','fps=%d'%int(fps),'-ovc','lavc','-lavcopts','vbitrate=%d:vpass=%d:threads=%d:%s'%(int(kbps),passNo,woo.master.numThreads,'turbo' if passNo==1 else ''),'-passlogfile',passLogFile,'-o',(devNull if passNo==1 else out)]
            elif encType=='avconv':
                #inputs=sum([['-i',f] for f in frameSpecAvconv],[])
                # inputs=['-i','concat:"'+'|'.join(frameSpecAvconv)+'"']
                inputs=['-i',symPattern]
                pix_fmt=['-pix_fmt','yuv420p'] # https://bugzilla.mozilla.org/show_bug.cgi?id=1368063
                cmd=[encExec]+inputs+['-r',str(int(fps)),'-b:v','%dk'%int(kbps),'-threads',str(woo.master.numThreads)]+pix_fmt+(['-pass',str(passNo),'-passlogfile',passLogFile] if passNo>0 else [])+['-an','-vf','crop=(floor(in_w/2)*2):(floor(in_h/2)*2)']+(['-f','rawvideo','-y',devNull] if passNo==1 else ['-f','mp4','-y',out])
            log.info('Pass %d: %s'%(passNo,' '.join(cmd)))
            ret=subprocess.call(cmd)
            if ret!=0: raise RuntimeError("Error running %s."%encExec)
    
    # clean up symlinks directory
    if encType=='avconv' or encType=='IMG':
        import shutil
        shutil.rmtree(symDir)

    if open:
        import webbrowser
        webbrowser.open('file://'+os.path.abspath(out))

def _procStatus(name):
    import os
    for l in open('/proc/%d/status'%os.getpid()):
        if l.split(':')[0]==name: return l
    raise "No such line in /proc/[pid]/status: "+name
def vmData():
    "Return memory usage data from Linux's /proc/[pid]/status, line VmData."
    l=_procStatus('VmData'); ll=l.split(); assert(ll[2]=='kB')
    return int(ll[1])


if sys.platform=='win32':
    def fixWindowsPath(p):
        '''On Windows, replace some well-known UNIX directories by their native equivalents,
        so that defaults works cross-platform. Currently, the following substitutions are made:
        1. leading `/tmp` is transformed to `%TEMP%` (or c:/temp is undefined)
        '''
        if p=='/tmp' or p.startswith('/tmp/'):
            temp=os.environ.get('TEMP','c:/temp')
            return temp+p[4:]
        return p
else:
    def fixWindowsPath(p): return p
    
    


def xMirror(half):
    """Mirror a sequence of 2d points around the x axis (changing sign on the y coord).
The sequence should start up and then it will wrap from y downwards (or vice versa).
If the last point's x coord is zero, it will not be duplicated."""
    return list(half)+[(x,-y) for x,y in reversed(half[:-1] if half[-1][0]==0 else half)]

def tesselatePolyline(l,maxDist):
    '''Return polyline tesselated so that distance between consecutive points is no more than maxDist. *l* is list of points (Vector2 or Vector3).'''
    ret=[]
    for i in range(len(l)-1):
        x0,x1=l[i],l[i+1]
        dX=x1-x0
        dist=dX.norm()
        if dist>maxDist:
            nDiv=int(dist/maxDist+(1 if int(dist/maxDist)*maxDist<dist else 0))
            for j in range(int(nDiv)): ret.append(x0+dX*(j*1./nDiv))
        else: ret.append(x0)
    ret.append(l[-1])
    return ret


def htmlReportHead(S,headline,dialect='xhtml',repBase='',hideWooExtra=False):
    '''
        Return (X)HTML fragment for simulation report: (X)HTML header, title, Woo logo, configuration and preprocessor parameters. 

        In order to obtain a well-formed XHTML document, don't forget to add '</body></html>'.
    '''
    import time, platform, pkg_resources, shutil
    import base64
    headers=dict(xhtml='''<?xml version="1.0" encoding="UTF-8"?>
    <!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd"> 
    <html xmlns="http://www.w3.org/1999/xhtml" xmlns:svg="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink">\n''',html5='<!DOCTYPE html><html lang="en">',html4='<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd"><html>')
    logoSrc=pkg_resources.resource_filename('woo','data/woo-icon.128.png')
    logoExtern='%s_wooLogo.png'%(repBase)
    logoBase64='<img src="data:image/png;base64,'+(str(base64.b64encode(open(logoSrc,'rb').read())))+'" alt="Woo logo"/>'
    logoImgExtern='<img src="%s_wooLogo.png" alt="Woo logo"/>'%(repBase)
    svgLogos=dict(
        # embedded svg
        xhtml=svgFileFragment(pkg_resources.resource_filename('woo','data/woodem-6.woo.svg')),
        # embedded base64-encoded pngs
        html5=logoBase64,
        html4=logoImgExtern,
    )
    if dialect not in headers: raise ValueError("Unsupported HTML dialect '%s'; must be one of: "+', '.join(headers.keys()))
    if dialect=='html4': shutil.copyfile(logoSrc,logoExtern)
    # <meta http-equiv="content-type" content="text/html;charset=UTF-8" />
    html=(u'''<head><title>{headline}</title><meta charset="utf-8"/></head><body>
        <h1>{headline}</h1>
        <h2>General</h2>
        <table style="text-align: center">
            <tr><td rowspan="3">{svgLogo}</td><td><a href="http://www.woodem.eu/">Woo[dem]</a></td></tr>
            <tr><td>© <a href="mailto:vaclav.smilauer@woodem.eu">Václav Šmilauer</a></td></tr>
            <tr><td>Licensed under <a href="http://opensource.org/licenses/gpl-2.0.php">GNU GPL v2</a></td></tr>
        </table>
        <table>
            <tr><td>title</td><td align="right">{title}</td></tr>
            <tr><td>id</td><td align="right">{id}</td></tr>
            <tr><td>started</td><td align="right">{started}</td></tr>
            <tr><td>duration</td><td align="right">{duration:g} s ({step} steps)</td></tr>
            <tr><td>average speed</td><td align="right">{stepsPerSec:g} steps/sec</td></tr>
            <tr><td>operator</td><td align="right">{user}</td></tr>
            <tr><td>number of cores used</td><td align="right">{nCores}</td></tr>
            <tr><td>engine</td><td align="right">{engine}</td></tr>
            <tr><td>compiled with</td><td align="right">{compiledWith}</td></tr>
            <tr><td>platform</td><td align="right">{platform}</td></tr>
        </table>
        '''.format(headline=headline,title=(S.tags['title'] if S.tags['title'] else '<i>[none]</i>'),id=S.tags['id'],user=(S.tags['user']),started=time.ctime(time.time()-woo.master.realtime),step=S.step,duration=woo.master.realtime,nCores=woo.master.numThreads,stepsPerSec=S.step/woo.master.realtime,engine='wooDem '+woo.config.version+'/'+woo.config.revision+(' (debug)' if woo.config.debug else ''),compiledWith=','.join(woo.config.features),platform=platform.platform().replace('-',' '),svgLogo=svgLogos[dialect])
        +'<h2>Input data</h2>'+S.pre.dumps(format='html',fragment=True,showDoc=False,hideWooExtra=hideWooExtra)
    )
    return headers[dialect]+html

def htmlReport(S,repFmt,headline,afterHead='',figures=[],dialect=None,figFmt=None,svgEmbed=False,show=False,hideWooExtra=False):
    '''
    Generate (X)HTML report for simulation.

    :param S: :obj:`Scene` object; must contain :obj:`Scene.pre`
    :param repFmt: format for the output file; will be expanded using :obj:`Scene.tags`; it should end with `.html` or `.xhtml`.
    :param headline: title of the report, used as HTML title and the first ``<h1>`` heading.
    :param afterHead: contents (XHTML fragment) to be added verbatim after the header (title, woo config table, preprocessor parameters)
    :param figs: figures included in the report; they are either embedded (SVG, optionally ) or saved to files in the same directory as the report, using its name as prefix, and referenced via relative links from the XHTML. Figures are given as list of 2- or 3-tuples, where each item contains:
       
         1. name of the figure (will generate a ``<h2>`` heading)
         2. figure object (`Matplotlib <http://matplotlib.org>`__ figure object)
         3. optionally, format to save the figure to (`svg`, `png`, ...)

    :param figFmt: format of figures, if the format is not specified by the figure itself; if None, choose format appropriate for given dialect (png for html4, svg for html5 and xhtml)
    :param dialect: one of "xhtml", "html5", "html4"; if not given (*None*), selected from file suffix (``.html`` for ``html4``, ``.xhtml`` for ``xhtml``). ``html4`` will save figures as PNG (even if something else is specified) so that the resulting file is easily importable into LibreOffice (which does not import HTML with SVG correctly now).
    :param svgEmbed: don't save SVG as separate files, embed them in the report instead. 
    :param show: open the report in browser via the webbrowser module, unless running in batch.
    :param hideWooExtra: don't show wooExtra.* modules in object names or URLs
    :return: (filename of the report, list of external figures)

    '''
    import codecs, re, os.path
    import woo,woo.batch
    dialects=set(['html5','html4','xhtml'])
    if not dialect:
        if repFmt.endswith('.xhtml'): dialect='xhtml'
        elif repFmt.endswith('.html'): dialect='html4'
        else: raise ValueError("Unable to guess dialect (not given) from *repFmt* '%s' (not ending in .xhtml or .html)")
    if dialect not in dialects: raise ValueError("Unknown dialect '%s': must be one of "+', '.join(dialects)+'.')
    if figFmt==None: figFmt={'html5':'png','html4':'png','xhtml':'svg'}[dialect]
    if dialect in ('html5',) and svgEmbed and figFmt=='svg':
        svgEmbed=False
        warnings.warn("Dialect '%s' does not support embedded SVG -- will not be embedded."%dialect)
    if dialect in ('html4',) and not figFmt=='png':
        warnings.warn("Dialect '%s' will save images as 'png', not '%s'"%(dialect,figFmt))
        figFmt='png'
    repName=str(repFmt).format(S=S,**(dict(S.tags)))
    rep=codecs.open(repName,'w','utf-8','replace')
    log.info('Writing report to file://'+os.path.abspath(repName))
    repBase=re.sub(r'\.x?html$','',repName)
    s=htmlReportHead(S,headline,dialect=dialect,repBase=repBase,hideWooExtra=hideWooExtra)
    s+=afterHead


    if figures: s+='<h2>Figures</h2>\n'
    extFiles=[]
    from matplotlib.backends.backend_agg import FigureCanvasAgg
    for ith,figspec in enumerate(figures):
        if len(figspec)==2: figspec=(figspec[0],figspec[1],figFmt)
        figName,figObj,figSuffix=figspec
        if not figName: figName='Figure %i'%(ith+1)
        s+='<h3>'+figName+'</h3>'
        canvas=FigureCanvasAgg(figObj)
        if figSuffix=='svg' and svgEmbed:
            figFile=woo.master.tmpFilename()+'.'+figSuffix
            figObj.savefig(figFile)
            s+=svgFileFragment(figFile)
        else: 
            figFile=repBase+'.'+re.sub('[^a-zA-Z0-9_-]','_',figName)+'.'+figSuffix
            figObj.savefig(figFile)
            s+='<img src="%s" alt="%s"/>'%(os.path.basename(figFile),figName)
            extFiles.append(os.path.abspath(figFile))
    s+='</body></html>'
    rep.write(s)
    rep.close() # flushed write buffers

    # attempt conversion to ODT, not under Windows
    if False and sys.platform!='win32':
        # TODO TODO TODO: have timeout on the abiword call; some versions stall forever
        def convertToOdt(html,odt):
            try:
                import subprocess
                # may raise OSError if the executable is not found
                out=subprocess.check_output(['abiword','--to=ODT','--to-name='+odt,html])
                print('Report converted to ODT via Abiword: file://'+os.path.abspath(odt))
            except subprocess.CalledProcessError as e:
                print('Report conversion to ODT failed (error when running Abiword), returning %d; the output was: %s.'%(e.returncode,e.output))
            except:
                print('Report conversion to ODT not done (Abiword not installed?).')
        # if this is run in the background, main process may finish before that thread, leading potentially to some error messages??
        import threading
        threading.Thread(target=convertToOdt,args=(repName,repBase+'.odt')).start()
                
    if show and not woo.batch.inBatch():
        import webbrowser
        webbrowser.open('file://'+os.path.abspath(repName))
    return repName,extFiles


def svgFileFragment(filename):
    'Return fragment beginning with ``<svg`` till the end of given *filename*. Those data may be used for embedding SVG in other formats, such as XHTML.'
    import codecs,re
    data=codecs.open(filename,encoding='utf-8').read()
    # returns the first match
    for m in re.finditer(r'<svg( |$)',data,flags=re.MULTILINE): return data[m.start():]
    raise RuntimeError('"<svg" was not found in %s'%filename)
    


def ensureWriteableDir(d):
    'Make sure given directory is writeable; raise exception (IOError) if not.'
    import errno, os
    try:
        os.makedirs(d)
        print('Created directory:',d)
    except OSError as e:
        if e.errno!=errno.EEXISTS:
            log.error('failed to create directory '+d)
            raise
    if not os.access(d,os.W_OK): raise IOError('Directory %s not writeable.'%d)


def runAllPreprocessors():
    'Run all preprocessors with default parameters, and run the very first step of their simulation.'
    for P in woo.system.childClasses(woo.core.Preprocessor):
        print(50*'#'+'\n'+10*' '+P.__module__+'.'+P.__name__+'\n'+50*'#')
        S=woo.master.scene=P()()
        S.one
    print(20*'*'+'   ALL OK   '+20*'*')

def unbalancedEnergy(S):
    if not S.trackEnergy: raise RuntimeError('Scene.trackEnergy==False')
    if not 'elast' in S.energy or 'kinetic' not in S.energy: return float('nan')
    #raise RuntimeError('Scene.energy: does not contain *elast* or *kinetic* keys')
    # this line seems to be necessary under Windows, zero raises ZeroDivisionError...?!
    if S.energy['elast']==0: return float('nan')
    return S.energy['kinetic']/S.energy['elast']


def freecadExport(S,out,mask=0):
    import textwrap, math
    with open(out,'w') as f:
        f.write(textwrap.dedent('''\
            import FreeCAD, Part
            Vector,Rotation,Placement=FreeCAD.Base.Vector,FreeCAD.Base.Rotation,FreeCAD.Base.Placement
            doc=FreeCAD.newDocument()
        '''))
        def node2placement(n):
            'Return textual representation of position and orientation of the :obj:`woo.core.Node` object.'
            pos,(axis,rot)=n.pos,n.ori.toAxisAngle()
            return 'Placement(Vector(%g,%g,%g),Rotation(Vector(%g,%g,%g),%g))'%(pos[0],pos[1],pos[2],axis[0],axis[1],axis[2],rot*180/math.pi)
        for p in S.dem.par:
            if not p.shape or (mask and (p.mask&mask==0)): continue
            name='par_%05d'%p.id
            if isinstance(p.shape,woo.dem.Sphere):
                f.write(textwrap.dedent('''\
                    p=doc.addObject("Part::Sphere","{name}")
                    p.Radius={r}
                    p.Placement={placement}
                ''').format(name=name,r=p.shape.radius,placement=node2placement(p.shape.nodes[0])))
            elif isinstance(p.shape,woo.dem.Ellipsoid):
                f.write(textwrap.dedent('''\
                    p=doc.addObject("Part::Ellipsoid","{name}")
                    p.Radius1,p.Radius2,p.Radius3={r[0]},{r[1]},{r[2]}
                    ''').format(name=name,r=p.shape.semiAxes,placement=node2placement(p.shape.nodes[0])))
            else: warnings.warn('S.dem.par[%d] not exported (unhandled shape %s)'%(p.id,p.shape.__class__.__name__))
        f.write('doc.recompute()\n')

def waitWithProgress(S,refresh=.5):
    import rich.panel
    import rich.progress
    import time
    import psutil
    class WooProgress(rich.progress.Progress):
        def __init__(self,S):
            self.S=S
            if S.stopAtTime>0: self.proType='time'
            elif S.stopAtStep>0: self.proType='step'
            else: self.proType='indet'
            self.colGoal=rich.progress.TextColumn('')
            self.proc=psutil.Process(os.getpid())
            self.proc.cpu_percent() # ignore value of the first call, is zero
            super().__init__(
                rich.progress.SpinnerColumn(),
                self.colGoal,
                rich.progress.BarColumn(),
                rich.progress.TaskProgressColumn(show_speed=True),
                rich.progress.TimeElapsedColumn(),
                rich.progress.TimeRemainingColumn(),
                auto_refresh=True,
                refresh_per_second=1./refresh,
                expand=True
            )
            self.myTask=self.add_task('Running...',total={'time':S.stopAtTime,'step':S.stopAtStep,'indet':None}[self.proType])
        def update(self):
            if not hasattr(self,'myTask'): return
            super().update(self.myTask,completed={'time':S.time,'step':S.step,'indet':S.step}[self.proType])
            # super().refresh()
        def get_renderables(self):
            self.update()
            self.description='t={S.t} | Δt={S.dt}'
            if self.proType=='time': self.colGoal.text_format=f'time [progress.elapsed]{S.time:<6f}[/progress.elapsed] / [progress.filesize]{S.stopAtTime:.6g} s[/progress.filesize]'
            elif self.proType=='step': self.colGoal.text_format=f'step [progress.elapsed]{S.step}[/progress.elapsed] / [progress.filesize]{S.stopAtStep}[/progress.filesize]'
            else: self.colGoal.text_format=''
            task_table=self.make_tasks_table(self.tasks)
            task_table.title=f'DEM ▶ t={S.time:.6f} step={S.step} ● Δt={S.dt:.6f} par={len(S.dem.par)} con={len(S.dem.con)}'
            task_table.caption=f'CPU ▶ [green]{self.proc.cpu_percent():.3g}%[/green] ([red]{woo.master.numThreads}[/red] OpenMP threads)'
            yield rich.panel.Panel(task_table)
    S.run()
    with WooProgress(S=S) as progress:
        S.wait()


#############################
##### deprecated functions


def _deprecatedUtilsFunction(old,new):
    "Wrapper for deprecated functions, example below."
    warnings.warn('Function utils.%s is deprecated, use %s instead.'%(old,new),stacklevel=2,category=DeprecationWarning)

# example of _deprecatedUtilsFunction usage:
#
# def import_mesh_geometry(*args,**kw):
#    "|ydeprecated|"
#    _deprecatedUtilsFunction('import_mesh_geometry','woo.import.gmsh')
#    import woo.ymport
#    return woo.ymport.stl(*args,**kw)

import woo.batch

def runningInBatch():
    _deprecatedUtilsFunction('runningInBatch','woo.batch.inBatch')
    return woo.batch.inBatch()
def waitIfBatch():
    _deprecatedUtilsFunction('waitIfBatch','woo.batch.wait')
    return woo.batch.wait()
def readParamsFromTable(*args,**kw):
    _deprecatedUtilsFunction('readParamsFromTable','woo.batch.readParamsFromTable')
    return woo.batch.readParamsFromTable(*args,**kw)
def runPreprocessorInBatch(*args,**kw):
    _deprecatedUtilsFunction('runPreprocessorInBatch','woo.batch.runPreprocessor')
    return woo.batch.runPreprocessor(*args,**kw)


