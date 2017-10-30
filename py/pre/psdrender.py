# encoding: utf-8

from __future__ import print_function
from woo.dem import *
from woo.fem import *
import woo.core
import woo.dem
import woo.pyderived
import woo.models
import math
from minieigen import *


class PsdRender(woo.core.Preprocessor,woo.pyderived.PyWooObject):
    '''Preprocessor for creating layer with given PSD and passing it to POV-Ray for rendering.'''
    _classTraits=None
    _PAT=woo.pyderived.PyAttrTrait
    _attrTraits=[
        _PAT(Vector2,'size',(.2,.3),unit='m',doc='Region to be rendered (the particle area will be larger by :obj:`sizeExtraRel`'),
        _PAT(float,'sizeExtraRel',.2,doc='Enlarge particle area by this much, relative to size.'),
        _PAT(woo.dem.ParticleGenerator,'gen',woo.dem.PsdSphereGenerator(psdPts=[(.007,0),(0.009,.2),(0.012,.5),(0.016,.9),(.02,1)]),doc='Particle generator'),
        _PAT(float,'relHt',2,doc='Bed height relative to largest particle diameter (computed via mass, supposing porosity of :obj:`porosity`)'),
        _PAT(Vector3,'gravity',(0,0,-9.81),doc='Gravity acceleration.'),
        _PAT(float,'porosity',.4,doc='Approximate porosity to convert bed height (:obj:`relHt`) to mass; it will not influence porosity itself, which is given almost exclusively geometrically.'),
        _PAT(int,'stepPeriod',200,doc='Periodicity for the factory.'),
        _PAT(float,'maxUnbalanced',.2,doc='Unbalanced force/energy to wait for before declared settled.'),
        _PAT(woo.models.ContactModelSelector,'model',woo.models.ContactModelSelector(name='linear',damping=.5,numMat=(1,1),matDesc=['particles'],mats=[woo.dem.FrictMat(density=2e3,young=5e6,tanPhi=0.)]),doc='Contact model and material type; since the simulation is trivial, this has practically no influence, except of friction: less (or zero) friction will make the packing more compact.'),
    ]
    def __init__(self,**kw):
        woo.core.Preprocessor.__init__(self)
        self.wooPyInit(self.__class__,woo.core.Preprocessor,**kw)
    def __call__(self):
        woo.master.usesApi=10104
        pre=self
        S=woo.core.Scene(fields=[woo.dem.DemField(gravity=pre.gravity)],pre=pre.deepcopy())
        S.trackEnergy=True
        mat=pre.model.mats[0]
        lxy=(1+pre.sizeExtraRel)*pre.size
        bedHt=pre.relHt*pre.gen.minMaxDiam()[1]
        boxHt=2*bedHt
        mass=bedHt*(1-pre.porosity)*mat.density*lxy[0]*lxy[1]
        print('Need to generate %g kg to create layer of %g m (ρ=%g kg/m³, solid ratio %g)'%(mass,bedHt,mat.density,1-pre.porosity))
        box=AlignedBox3((-lxy[0]/2,-lxy[1]/2,0),(lxy[0]/2,lxy[1]/2,2*bedHt))
        S.dem.par.add(woo.dem.Wall.makeBox(box,mat=mat))
        S.engines=woo.dem.DemField.minimalEngines(model=pre.model)+[
            woo.dem.BoxInlet(box=box,generator=pre.gen,maxMass=mass,massRate=0,stepPeriod=pre.stepPeriod,label='inlet',atMaxAttempts='ignore',doneHook='engine.dead=True',materials=[mat]),
            woo.core.PyRunner(pre.stepPeriod,'woo.pre.psdrender.checkSettled(S)')
        ]
        return S

def checkSettled(S):
    if not S.lab.inlet.dead: return
    #unb=woo.utils.unbalancedEnergy(S)
    #print('Unbalanced energy: ',unb)
    unb=woo.utils.unbalancedForce(S)
    print('Unbalanced force:',unb)
    if unb>S.pre.maxUnbalanced: return
    else: finalize(S)

def finalize(S):
    S.stop()
    print('Finished!!!')
    pov=POVRayExport(masks=[woo.dem.DemField.defaultOutletBit],textures=['particles'],out='/tmp/aaa')
    pov(S)





