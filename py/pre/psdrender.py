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
        _PAT(float,'dtSafety',.7,doc='Safety factor for timestep.'),
        _PAT(float,'maxUnbalanced',.5,doc='Unbalanced force/energy to wait for before declared settled.'),
        _PAT(woo.models.ContactModelSelector,'model',woo.models.ContactModelSelector(name='linear',damping=.5,numMat=(1,1),matDesc=['particles'],mats=[woo.dem.FrictMat(density=2e3,young=5e6,tanPhi=0.)]),doc='Contact model and material type; since the simulation is trivial, this has practically no influence, except of friction: less (or zero) friction will make the packing more compact.'),
        _PAT(bool,'povEnable',True,doc='Enable POV-Ray export.'),
        _PAT(Vector3,'camPos',(0,0,.5),doc='Camera position; x, y components determine offset from bed normal; z is height above the highest particle center.'),
        _PAT(str,'out','/tmp/{tid}',doc='Prefix for outputs (:obj:`woo.core.Scene.tags` are expanded).'),
        _PAT(int,'imgDim',2000,doc='Larger image dimension when the scene is rendered; the scene can be re-rendered by calling POV-Ray by hand anytime with arbitrary resolution.'),
        _PAT(str,'povLights','''// light_source{<-8,-20,30> color rgb .75}
        // light_source{<25,-12,12> color rgb .44}
        light_source{<-.5,-.5,10> color White area_light <1,0,0>,<0,1,0>, 5, 5 adaptive 1 jitter }
        ''',doc='Default light specifications for POV-Ray'),
        _PAT(bool,'saveShapePack',True,'Create a .shapepack file with particle geometries.'),

    ]
    def __init__(self,**kw):
        woo.core.Preprocessor.__init__(self)
        self.wooPyInit(self.__class__,woo.core.Preprocessor,**kw)
    def __call__(self):
        woo.master.usesApi=10104
        pre=self
        S=woo.core.Scene(fields=[woo.dem.DemField(gravity=pre.gravity)],pre=pre.deepcopy(),dtSafety=pre.dtSafety)
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
    import math, subprocess, os.path
    S.stop()
    print('Finished.')
    out2=S.expandTags(S.pre.out)
    # do not export walls at all
    pov=POVRayExport(mask=woo.dem.DemField.defaultOutletBit,masks=[woo.dem.DemField.defaultOutletBit],textures=['particles'],out=out2)
    if S.pre.saveShapePack:
        sp=woo.dem.ShapePack()
        sp.fromDem(S,S.dem,mask=woo.dem.DemField.defaultOutletBit)
        sp.saveTxt(out2+'.shapepack')
    if S.pre.povEnable:
        bedZ=max([p.pos[2] for p in S.dem.par])
        camAngle=math.degrees(2*math.atan(.5*min(S.pre.size)/S.pre.camPos[2]))
        out2base=os.path.basename(out2).encode()
        pov(S)
        pmaster=b'''
#version 3.7;
#include"colors.inc"
#include"metals.inc"
#include"textures.inc"
global_settings{ assumed_gamma 2.2 }
camera{
    location<%g,%g,%g>
    sky y
    right y*image_width/image_height
    up y
    look_at <0,0,0>
    angle %g
}
background{rgb .2}
'''%(S.pre.camPos[0],S.pre.camPos[1],bedZ+S.pre.camPos[2],camAngle)+S.pre.povLights.encode()+b'''
#declare o=<0,0,0>;
#declare nan=1e12;

#declare RndTexA=seed(12345);
#declare RndTexB=seed(54321);
#declare RndTexC=seed(23659);
#declare RndTexD=seed(33333);

#macro woo_tex_particles(rgb0,rgb1,diam)
   #declare dark=.7;
   #if(diam<9e-3) #declare c=<1,dark,dark>;
   #else
      #if(diam<16e-3) #declare c=<dark,1,dark>;
      #else #declare c=<dark,dark,1>;
      #end
   #end
   #declare c=<1,1,1>;
   texture{
      pigment { crackle form <-1,1,0> scale 1e-4 metric 2 color_map { [0 color .6*c] [1 color c] } }
      normal  { granite .1 scale 1e-2 turbulence .5 bump_size .4 translate <rand(RndTexA),rand(RndTexB),rand(RndTexC)> }
      finish  { reflection .001 specular .00 ambient .3 diffuse .5 phong 1 phong_size 50 }
   }
#end

#include "%s_static.inc"
#include concat(concat("%s_frame_",str(frame_number,-5,0)),".inc")

'''%(out2base,out2base)
        outMaster=out2+'_master.pov'
        print('Writing out to '+outMaster)
        with open(outMaster,'wb') as m: m.write(pmaster)
        ratio=S.pre.size[0]/S.pre.size[1]
        if ratio>1: px=(S.pre.imgDim,int(S.pre.imgDim/ratio))
        else: px=(int(S.pre.imgDim*ratio),S.pre.imgDim)
        cmd=['povray','+W%d'%px[0],'+H%d'%px[1],'+A','-kff0',outMaster]
        print('Running: cd '+os.path.dirname(out2)+'; povray '.join(cmd))
        subprocess.call(cmd,cwd=os.path.dirname(out2))
    







