import woo.dem, woo.core, woo.pyderived, woo.models, woo.triangulated
from minieigen import *
import numpy, math


class DissipChute(woo.core.Preprocessor,woo.pyderived.PyWooObject):
    '''Chute for studying dissipation of energy.'''
    _PAT=woo.pyderived.PyAttrTrait
    _attrTraits=[
        _PAT(Vector3,'dim',Vector3(.2,.2,2),unit='m',doc='Chute dimensions: width, depth, height'),
        _PAT(int,'finNum',5,doc='Number of fins protruding from the sides (alternating from left/right'),
        _PAT(float,'finLen',.15,unit='m',doc='Length of fins'),
        _PAT(float,'finSlope',30*woo.unit['deg'],unit='deg',doc='Slope of fins (positive downwards).'),
        _PAT(woo.dem.ParticleGenerator,'gen',woo.dem.PsdSphereGenerator(psdPts=[(8e-3,0),(12e-3,.40),(20e-3,1.)]),'Particle generator, placed above the chute'),
        _PAT(woo.models.ContactModelSelector,'model',woo.models.ContactModelSelector(name='luding',numMat=(1,2),matDesc=['feed','walls'],mats=[woo.dem.LudingMat(young=3e6,k1DivKn=.5,kaDivKn=.2,ktDivKn=.2,krDivKn=.2,kwDivKn=.2,deltaLimRel=.1,dynDivStat=.7,tanPhi=.4,statR=.2,statW=.2,viscN=.1,viscT=.1,viscR=.1,viscW=0)]),doc='Contact model'),
        _PAT(float,'feedRate',5,unit='kg/s',doc='Feed mass rate'),
        _PAT(float,'feedTime',.5,unit='s',doc='Time for which the feed is activated'),
        _PAT(float,'stopTime',2,unit='s',doc='Time when to stop the simulation'),
        _PAT(int,'vtkStep',400,doc='Interval to run VTK export'),
        _PAT(str,'vtkOut','/tmp/{tid}',doc='Output for VTK files. Tags will be expanded. If empty, VTK export will be disabled.'),
        _PAT(float,'dtSafety',.7,doc=':obj:`woo.core.Scene.dtSafety`'),
        _PAT(str,'saveDone','',doc='File to same simulation at the end; not saved if empty.'),
    ]

    def __init__(self,**kw):
        woo.core.Preprocessor.__init__(self)
        self.wooPyInit(DissipChute,woo.core.Preprocessor,**kw)
    def __call__(self):
        woo.master.usesApi=10103
        pre=self
        S=woo.core.Scene(fields=[woo.dem.DemField(gravity=(0,0,-10))],dtSafety=pre.dtSafety)
        S.pre=pre.deepcopy()
        wallMat=pre.model.mats[1 if len(pre.model.mats)>1 else 0]
        mat=pre.model.mats[0]
        x2,y2=.5*pre.dim[0],.5*pre.dim[1]
        htBrut=pre.dim[2]+x2+y2
        htNet=pre.dim[2]
        dp=2*(-x2-y2) # outlet height
        mask=woo.dem.DemField.defaultStaticMask
        
        # chute itself
        S.dem.par.add(woo.triangulated.box(dim=(pre.dim[0],pre.dim[1],htBrut),center=(0,0,.5*htBrut),which=Vector6(1,1,0,1,1,0),mat=wallMat,mask=mask))
        # ribs
        for i,z in enumerate(list(numpy.linspace(0,htNet,pre.finNum+1,endpoint=False))[1:]):
            sgn=(-1 if i%2 else +1)
            A,B=[Vector3(sgn*x2,y,z) for y in (y2,-y2)]
            C,D=[Vector3(sgn*x2-sgn*pre.finLen*math.cos(pre.finSlope),y,z-pre.finLen*math.sin(pre.finSlope)) for y in (y2,-y2)]
            S.dem.par.add(woo.triangulated.quadrilateral(A,B,C,D,mat=wallMat,mask=mask))
        inlet=woo.dem.BoxInlet(
            generator=pre.gen,
            stepPeriod=400,
            massRate=pre.feedRate,
            maxMass=pre.feedTime*pre.feedRate,
            materials=[mat],
            matStates=[woo.dem.LudingMatState()],
            box=((-x2,-y2,htNet),(x2,y2,htBrut)),
        )
        outlet=woo.dem.BoxOutlet(
            stepPeriod=200,
            box=((-2*x2,-2*y2,dp),(2*x2,2*y2,0)),
            inside=True
        )
        S.engines=woo.dem.DemField.minimalEngines(model=pre.model)+[inlet,outlet]
        S.engines+=[woo.core.PyRunner(400,'S.plot.addData(i=S.step,t=S.time,Etot=S.energy.total(),Eerr=(S.energy.relErr() if S.step>200 else float("nan")),**S.energy)')]
        # vtk
        if pre.vtkStep and pre.vtkOut: S.engines+=[woo.dem.VtkExport(out=pre.vtkOut,stepPeriod=pre.vtkStep)]

        S.stopAtTime=pre.stopTime
        S.stopAtHook='S.pre.finalize(S)'

        S.dem.saveDead=True
        S.engines+=[woo.dem.Tracer(stepPeriod=100,realPeriod=0,saveTime=True,compress=2,num=400,compSkip=2,minDist=1e-3*htBrut,scalar='matState.getScalar',matStateIx=0,matStateSmooth=0.01)]

        S.plot.plots={'i':('Etot','**S.energy'),' t':('Eerr')}

        S.trackEnergy=True
        S.energy.gridOn(box=AlignedBox3((-x2,-y2,dp),(x2,y2,htNet)),cellSize=htNet*1e-2,maxIndex=10)
        S.lab.collider.sortAxis=2 # faster for predominantly vertical arrangement

        return S

    def finalize(self,S):
        import woo.paraviewscript
        woo.paraviewscript.fromEngines(S,out=S.pre.vtkOut,launch=(not woo.batch.inBatch()))
        if S.pre.saveDone: S.save(S.expandTags(S.pre.saveDone))


if __name__ in ('__main__','wooMain'):
    S=woo.master.scene=DissipChute()()
    S.run()
