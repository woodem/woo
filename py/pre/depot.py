# encoding: utf-8

from __future__ import print_function
from woo.dem import *
from woo.fem import *
import woo.core, woo.dem, woo.pyderived, woo.models, woo.config
import math
from minieigen import *

class CylDepot(woo.core.Preprocessor,woo.pyderived.PyWooObject):
    ''''Deposition of particles inside cylindrical tube. This preprocessor was created for pre-generation of dense packing inside cylindrical chamber, which is then exported to STL serving as input to `OpenFOAM <http://openfoam.org>`__ for computing permeability (pressure loss) of the layering.

    The target specimen (particle packing) dimensions are :obj:`htDiam` (cylinder height and diameter); the packing is inside cylindrical wall, with inlet at the bottom and outlet at the top; this cylinder may be longer than the specimen, which is set by :obj:`extraHt`.

    The height of compact packing is not known in advance precisely (it is a function of PSD, material, layering etc); the estimate is set by :obj:`relSettle`, which is used to compute :obj:`ht0`, the height of loose packing (where particles are initially generated), which then settles down in gravity. 

    The packing is settled when :obj:`unbalanced energy <woo.utils.unbalancedEnergy>` (the ratio of kinetic to elastic energy) drops below :obj:`unbE`.

    Once settled, particles are clipped to the required height. The actual value of settlement is computed and printed (so that it can be used iteratively as input for the next simulation).

    Layering specification uses the functionality of :obj:`woo.dem.LayeredAxialBias`, which distributed fractions along some axis in particle generation space. Suppose that we are to model the following two scenarios (which correspond to :obj:`preCooked` variants ``Brisbane 1`` and ``Brisbane 2``, which have spherical particles with piecewise-linear PSD distributed in layered fractions:

    .. image:: fig/depot-brisbane.*

    This arrangement is achieved with the following settings:

    1. ``Brisbane 1``:

        PSD is defined (via :obj:`PsdSphereGenerator.psdPts <woo.dem.PsdSphereGenerator.psdPts>`) to match relative height (and thus mass) of fractions (on the right), which is 0.1777 for the coarser fraction (12.5-20mm) and 0.8222 for the finer fraction. This we set by::
        
           psdPts=[(6.3e-3,0),(12.5e-3,.82222),(20e-3,1)]

        Layering is achieved by assigning :obj:`~woo.dem.LayeredAxialBias` to :obj:`bias`. The layers are distributed along the normalized height by setting :obj:`~woo.dem.LayeredAxialBias.layerSpec` to::
        
            [VectorX([12.5e-3,20e-3,0,.1777]),VectorX([0,12.5e-3,.1777,1]
             
        where each ``VectorX`` contains first minimum and maximum diameter, and at least one axial height range (in normalized coordinates).

    2. ``Brisbane 2``:

        PSD: set :obj:`~woo.dem.PsdSphereGenerator.psdPts` to::
        
           psdPts=[(6.3e-3,0),(12.5e-3,.4111),(20e-3,1)]

        Layering (:obj:`~woo.dem.LayeredAxialBias.layerSpec`) is set as::

           layerSpec=[VectorX([12.5e-3,20e-3, 0,.1777,.5888,1]),VectorX([0,12.5e-3, .177777,.58888]
            
        where the coarse fraction is distributed uniformly over both intervals in 0-0.1777 *and* 0.5888-1.0.

    Resulting heights of fractions vitally depend on :obj:`relSettlement`, so it may take some experimentation to get the result right:

    .. image:: fig/depot-brisbane-3d.png


    '''
    _classTraits=None
    _PAT=woo.pyderived.PyAttrTrait # less typing
    #defaultPsd=[(.007,0),(.01,.4),(.012,.7),(.02,1)]
    defaultPsd=[(5e-3,.0),(6.3e-3,.12),(8e-3,.53),(10e-3,.8),(12.5e-3,.94),(20e-3,1)]
    def postLoad(self,I):
        if self.preCooked and (I is None or I=='preCooked'):
            print('Applying pre-cooked configuration "%s".'%self.preCooked)
            if self.preCooked=='Brisbane 1':
                self.gen=woo.dem.PsdSphereGenerator(psdPts=[(6.3e-3,0),(12.5e-3,.82222),(20e-3,1)],discrete=False)
                self.bias=woo.dem.LayeredAxialBias(axis=2,fuzz=0,layerSpec=[VectorX([12.5e-3,1,0,.177777]),VectorX([0,12.5e-3,.177777,1])])
                self.relSettle=.38
            elif self.preCooked=='Brisbane 2':
                self.gen=woo.dem.PsdSphereGenerator(psdPts=[(6.3e-3,0),(12.5e-3,.41111),(20e-3,1)],discrete=False)
                self.bias=woo.dem.LayeredAxialBias(axis=2,fuzz=0,layerSpec=[VectorX([12.5e-3,1,0,.177777,.58888,1]),VectorX([0,12.5e-3,.177777,.58888])])
                self.relSettle=.37
            elif self.preCooked=='Lhasa 1':
                self.gen=woo.dem.PsdSphereGenerator(psdPts=[(8e-3,0),(16e-3,1.)],discrete=False)
                self.htDiam=(44e-3,140e-3)
                self.model.mats[0].density=3.8e3
                self.model.mats[0].young=1e4
                self.bias=None
                self.mass=.5
                self.relSettle=.4

            else: raise RuntimeError('Unknown precooked configuration "%s"'%self.preCooked)
            self.preCooked=''
        self.ht0=self.htDiam[0]/self.relSettle
        # if I=='estSettle': 
    _attrTraits=[
        _PAT(str,'preCooked','',noDump=True,noGui=False,startGroup='General',choice=['','Brisbane 1','Brisbane 2','Lhasa 1'],triggerPostLoad=True,doc='Apply pre-cooked configuration (i.e. change other parameters); this option is not saved.'),
        _PAT(Vector2,'htDiam',(.45,.1),unit='m',doc='Height and diameter of the resulting cylinder; the initial cylinder has the height of :obj:`ht0`, and particles are, after stabilization, clipped to :obj:`htDiam`, the resulting height.'),
        _PAT(float,'relSettle',.3,triggerPostLoad=True,doc='Estimated relative height after deposition (e.g. 0.4 means that the sample will settle around 0.4 times the original height). This value has to be guessed, as there is no exact relation to predict the amount of settling; 0.3 is a good initial guess, but it may depend on the PSD.'),
        _PAT(float,'mass',-1,doc='Mass generated (will be precise up to a single particle weight; when negative, generate as much as fits into the volume (see :obj:`ht0`)'),
        _PAT(float,'ht0',.9,guiReadonly=True,noDump=True,doc='Initial height (for loose sample), computed automatically from :obj:`relSettle` and :obj:`htDiam`.'),
        _PAT(woo.dem.ParticleGenerator,'gen',woo.dem.PsdSphereGenerator(psdPts=defaultPsd,discrete=False),'Object for particle generation'),
        _PAT(woo.dem.SpatialBias,'bias',woo.dem.PsdAxialBias(psdPts=defaultPsd,axis=2,fuzz=.1,discrete=True),doc='Uneven distribution of particles in space, depending on their radius. Use axis=2 for altering the distribution along the cylinder axis.'),
        _PAT(woo.models.ContactModelSelector,'model',woo.models.ContactModelSelector(name='linear',damping=.4,numMat=(1,1),matDesc=['everything'],mats=[woo.dem.FrictMat(density=2e3,young=2e5,tanPhi=0)]),doc='Contact model and materials.'),
        _PAT(int,'cylDiv',40,'Fineness of cylinder division'),
        _PAT(float,'unbE',0.0001,':obj:`Unbalanced energy <woo._utils2.unbalancedEnergy>` as criterion to consider the particles settled.'),
        _PAT(woo.triangulated.MeshImport,'holderMesh',None,doc='Import holder from STL file rather than triangulating one. The :obj:`htDiam` is still used to determine where to generate particles.'),
        # STL output
        _PAT(str,'stlOut','',startGroup='Triangulation output',filename=True,doc='Output file with triangulated particles (not the boundary); if empty, nothing will be exported at the end.'),
        _PAT(float,'stlTol',.2e-3,unit='m',doc='Tolerance for STL export (maximum distance between ideal shape and triangulation; passed to :obj:`_triangulated.spheroidsToStl`)'),
        _PAT(bool,'stlUnion',False,doc='Compute union of surfaces before exporting to STL (removing surface parts which are inside overlapping volumes). This also causes clusters to be exported as separate solids (will be configurable in the future).'),
        _PAT(bool,'stlCyl',True,doc='Export also cylinder surface (top, bottom and lateral surface are separate solids within the STL file.'),
        _PAT(str,'scadOut','',filename=True,doc='Output OpenSCAD script with union of all particles.'),
        _PAT(Vector2,'extraHt',(.5,.5),unit='m',doc='Extra height to be added to bottom and top of the resulting packing, when the new STL-exported cylinder is created.'),
        _PAT(float,'cylAxDiv',-1.,'Fineness of division of the STL cylinder; see :obj:`woo.triangulated.cylinder` ``axDiv``. The defaults create nearly-square triangulation'),
        _PAT(float,'dtSafety',.7,'Timestep safety coefficient.'),
    ]
    def __init__(self,**kw):
        woo.core.Preprocessor.__init__(self)
        self.wooPyInit(self.__class__,woo.core.Preprocessor,**kw)
    def __call__(self):
        pre=self
        self.postLoad(None) # ensure consistency
        mat=pre.model.mats[0]
        S=woo.core.Scene(
            pre=self.deepcopy(),
            trackEnergy=True, # for unbalanced energy tracking
            dtSafety=pre.dtSafety,
            fields=[
                DemField(
                    gravity=(0,0,-10),
                    par=woo.triangulated.cylinder(Vector3(0,0,0),Vector3(0,0,pre.ht0),radius=pre.htDiam[1]/2.,div=pre.cylDiv,capA=True,capB=False,wallCaps=True,mat=mat) if not pre.holderMesh else pre.holderMesh.doImport(mat=mat)
                )
            ],
            engines=DemField.minimalEngines(model=pre.model)+[
                woo.dem.CylinderInlet(
                    node=woo.core.Node((0,0,0),Quaternion((0,1,0),-math.pi/2)),
                    height=pre.ht0,
                    radius=pre.htDiam[1]/2.,
                    generator=pre.gen,
                    spatialBias=pre.bias,
                    maxMass=pre.mass,maxNum=-1,massRate=0,maxAttempts=2000,materials=pre.model.mats,glColor=float('nan'),
                    nDo=1 # place all particles at once, then let settle it all down
                ),
                woo.core.PyRunner(100,'import woo.pre.depot; S.pre.checkProgress(S)'),
            ],
        )
        if 'opengl' in woo.config.features: S.gl.demField.colorBy='radius'
        return S

    def checkProgress(self,S):
        u=woo.utils.unbalancedEnergy(S)
        print("unbalanced E: %g/%g"%(u,S.pre.unbE))
        if not u<S.pre.unbE: return

        r,h=S.pre.htDiam[1]/2.,S.pre.htDiam[0]
        # check how much was the settlement
        zz=woo.utils.contactCoordQuantiles(S.dem,[.999])
        print('Compaction done, settlement from %g (loose) to %g (dense); rel. %g, relSettle was %g.'%(S.pre.ht0,zz[0],zz[0]/S.pre.ht0,S.pre.relSettle))
        # delete everything above; run this engine just once, explicitly
        woo.dem.BoxOutlet(box=((-r,-r,0),(r,r,h)))(S,S.dem)
        S.stop()


        if not S.pre.stlOut:
            print('Not running STL export (stlOut empty)')
        else:
            n=woo.triangulated.spheroidsToSTL(S.pre.stlOut,S.dem,tol=S.pre.stlTol,merge=S.pre.stlUnion,solid="particles")
            if S.pre.stlCyl:
                # delete the triangulated cylinder
                for p in S.dem.par:
                    if isinstance(p.shape,Facet): S.dem.par.remove(p.id)
                # create a new (CFD-suitable) cylinder
                # bits for marking the mesh parts
                S.lab.cylBits=8,16,32
                cylMasks=[DemField.defaultBoundaryMask | b for b in S.lab.cylBits]
                S.dem.par.add(woo.triangulated.cylinder(Vector3(0,0,-S.pre.extraHt[0]),Vector3(0,0,S.pre.htDiam[0]+S.pre.extraHt[1]),radius=S.pre.htDiam[1]/2.,div=S.pre.cylDiv,axDiv=S.pre.cylAxDiv,capA=True,capB=True,wallCaps=False,masks=cylMasks,mat=S.pre.model.mats[0]))
                n+=woo.triangulated.facetsToSTL(S.pre.stlOut,S.dem,append=True,mask=S.lab.cylBits[0],solid="lateral")
                n+=woo.triangulated.facetsToSTL(S.pre.stlOut,S.dem,append=True,mask=S.lab.cylBits[1],solid="bottom")
                n+=woo.triangulated.facetsToSTL(S.pre.stlOut,S.dem,append=True,mask=S.lab.cylBits[2],solid="top")
            print('Exported %d facets to %s'%(n,S.pre.stlOut))

        if not S.pre.scadOut: print('Not running OpenSCAD export (scadOut empty)')
        else:
            n=0
            with open(S.pre.scadOut,'w') as scad:
                import time
                scad.write('// Generated from %s%s on %s\n'%(self.__class__.__module__,self.__class__.__name__,time.asctime()))
                scad.write('$fn=15; /* set arc triangulation precision here */\nunion(){\n')
                for p in S.dem.par:
                    if isinstance(p.shape,woo.dem.Sphere):
                        n+=1
                        scad.write('    translate([%g,%g,%g]) sphere(r=%g); // #%d\n'%(p.pos[0],p.pos[1],p.pos[2],p.shape.radius,p.id))
                scad.write('}\n');
            print('Exported %d spheres to %s'%(n,S.pre.scadOut))



