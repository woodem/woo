import woo
from woo import utils
from woo.core import *
from woo.dem import *
from woo import plot
from woo import *
import woo.log
woo.master.usesApi=10102
m=FrictMat(density=1e3,young=1e7,ktDivKn=.2,tanPhi=.5)
S=woo.master.scene=Scene(dtSafety=.1,
    fields=[DemField(gravity=(0,0,0),par=[
        Sphere.make((0,0,0),.5,fixed=True,wire=True,mat=m),
        Sphere.make((0,.999999,0),.5,fixed=True,wire=True,mat=m,vel=(1,0,0))
    ],parNodes=True
)])
# S.dem.par[1].vel=(1,0,0)
S.engines=DemField.minimalEngines(damping=.0,cp2=Cp2_FrictMat_FrictPhys(label='cp2'),law=Law2_L6Geom_FrictPhys_LinEl6(charLen=.1),dynDtPeriod=10)+[
    LawTester(ids=(0,1),abWeight=.5,smooth=1e-4,stages=[
            LawTesterStage(values=(0,1e-2,0,0,0,0),whats='......',until='tester.u[1]>=1',done='print "Shear displacement %s reached."%tester.u[1]'),
            #LawTesterStage(values=(1e-2,0,0,0,0,0),whats='vvv...',until='not C',done='print "Contact broken."'),
            #LawTesterStage(values=(-1e-2,0,0,0,0,0),whats='ivv...',until='stage.rebound',done='print "Rebound with v0=1e-2m/s"'),LawTesterStage(values=(1e-2,0,0,0,0,0),whats='vvv...',until='not C',done='print "Contact broken."'),
            #LawTesterStage(values=(-1e-3,0,0,0,0,0),whats='ivv...',until='stage.rebound',done='print "Rebound with v0=1e-3m/s"'),LawTesterStage(values=(1e-3,0,0,0,0,0),whats='vvv...',until='not C',done='print "Contact broken."'),
            #LawTesterStage(values=(-1e-4,0,0,0,0,0),whats='ivv...',until='stage.rebound',done='print "Rebound with v0=1e-4m/s"'),LawTesterStage(values=(1e-4,0,0,0,0,0),whats='vvv...',until='not C',done='print "Contact broken."'),
        ],
        done='tester.dead=True; S.stop(); print "Everything done, making myself dead and pausing."',
        label='tester'
    ),
    PyRunner(10,'import woo; dd={}; dd.update(**S.lab.tester.fuv()); dd.update(**S.energy); S.plot.addData(i=S.step,dist=(S.dem.par[0].pos-S.dem.par[1].pos).norm(),v1=S.dem.par[0].vel.norm(),v2=S.dem.par[1].vel.norm(),t=S.time,bounces=S.lab.tester.stages[S.lab.tester.stage].bounces,dt=S.dt,f1=S.dem.par[0].f,f2=S.dem.par[1].f,**dd)'),
]
#S.pause()
S.trackEnergy=True
#plot.plots={' i':(('fErrRel_xx','k'),None,'fErrAbs_xx'),'i ':('dist',None),' i ':(S.energy),'   i':('f_xx',None,'f_yy','f_zz'),'  i':('u_xx',None,'u_yy','u_zz'),'i  ':('u_yz',None,'u_zx','u_xy')}
S.plot.plots={'i':('u_xx',None,('f_xx','g--')),'  i':('u_yy',None,('f_yy','g--')),'u_yy':('f_yy','f1_x'),'i ':('**S.energy'),' i':('k_xx',None,('dt','g-'))}
S.plot.plot()
import woo.gl
S.gl=woo.gl.GlSetup(woo.gl.Gl1_DemField(glyph=woo.gl.Gl1_DemField.glyphForce))
S.saveTmp()
#S.run()
S.one()
#S.run(1000)
#O.reload()
