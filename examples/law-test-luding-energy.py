import woo, woo.plot, woo.utils
from woo import utils
from woo.core import *
from woo.dem import *
from woo import *
import woo.log
r=1e-2
delta=1e-4

m=LudingMat(
    young=30e9,k1DivKn=1,kaDivKn=0,ktDivKn=.2,krDivKn=0,kwDivKn=0,deltaLimRel=.01,
    tanPhi=.4,statR=0,statW=0,
    dynDivStat=.2,
    viscN=0,viscT=0,viscR=0,viscW=0
)
S=woo.master.scene=Scene(
   fields=[DemField(
        distFactor=1.,
        parNodes=True, # add nodes of those particles, even if they are fixed
        par=[Sphere.make((0,0,0),r,fixed=False,wire=True,mat=m),Sphere.make((0,2*r-delta,0),r,fixed=False,wire=True,mat=m)])
    ],
   engines=DemField.minimalEngines(model=woo.models.ContactModelSelector(name='luding'))+[
        LawTester(
            ids=(0,1),abWeight=0,smooth=1e-4,stages=[
                # compress
                #LawTesterStage(values=(-1e-1,0,0,0,0,0),whats='v.....',until='C.geom.uN<%g'%(-r*5e-3)),
                #LawTesterStage(values=(1e-1,0,0,0,0,0),whats='v.....',until='C.phys.force[0]>0'),
                #LawTesterStage(values=(-1e-1,0,0,0,0,0),whats='v.....',until='C.geom.uN<%g'%(-r*2e-2)),
                #LawTesterStage(values=(1e-1,0,0,0,0,0),whats='v.....',until='C.geom.uN>%g'%(-r*1e-4)),
                # shear while not moving in the normal sense
                #LawTesterStage(values=(0,1e-2,0,0,0,0),whats='vv....',until='C.phys.xiT.norm()>1e-3'),
                # unload in the normal sense
                # LawTesterStage(values=(1e-3,0,0,0,0,0),whats='v.....',until='not C'),
                LawTesterStage(values=(0,1e-2,0,0,0,0),whats='vvvvvv',until='S.time>1e-1'),
            ],
            done='S.stop()',label='tester'
        ),
        PyRunner(10,'C=S.dem.con[0]; S.plot.addData(i=S.step,time=S.time,f=C.phys.force,t=C.phys.torque,uN=C.geom.uN,vc=C.geom.vel,wc=C.geom.angVel,va=C.pA.vel,xa=C.pA.pos,wa=C.pA.angVel,vb=C.pB.vel,xb=C.pB.pos,wb=C.pB.angVel)')
    ],
    #plot=Plot(plots={'uN':('f_x'),'uN ':('deltaMax'),' uN':('normBranch')}),
    plot=Plot(plots={
        #'time':('wa_x','wa_y','wa_z','wb_x','wb_y','wb_z'),
        #'time ':('va_x','va_y','va_z','vb_x','vb_y','vb_z'),
        #' time':('wc_x','wc_y','wc_z','vc_x','vc_y','vc_z'),
        #'  time':('xa_x','xa_y','xa_z','xb_x','xb_y','xb_z'),
        'time  ':('f_x','f_y','f_z'),
    }),
    dtSafety=.01,
    trackEnergy=True
)
# woo.master.trackEnergy # =True
#s1=S.dem.par[1]
#s1.angVel=(1,0,0)
S.saveTmp()
S.run()
#O.reload()
