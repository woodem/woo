import woo, woo.plot, woo.utils
from woo import utils
from woo.core import *
from woo.dem import *
from woo import *
import woo.log
r=1e-3
m=LudingMat(
    young=30e9,plastDivKn=.5,adhDivKn=.2,ktDivKn=.2,krDivKn=0,kwDivKn=0,relPlastDepth=.01,
    tanPhi=.4,statR=.4,statW=.4,
    viscN=0,viscT=0,viscR=0,viscW=0
)
S=woo.master.scene=Scene(
   fields=[DemField(
        distFactor=1.5,
        parNodes=True, # add nodes of those particles, even if they are fixed
        par=[Sphere.make((0,0,0),r,fixed=True,wire=True,mat=m),Sphere.make((0,2*r,0),r,fixed=True,wire=True,mat=m)])
    ],
   engines=DemField.minimalEngines(model=woo.models.ContactModelSelector(name='luding'))+[
        LawTester(
            ids=(0,1),abWeight=.5,smooth=1e-4,stages=[
                # compress
                LawTesterStage(values=(-1e-1,0,0,0,0,0),whats='v.....',until='C.geom.uN<%g'%(-r*5e-3)),
                LawTesterStage(values=(1e-1,0,0,0,0,0),whats='v.....',until='C.phys.force[0]>0'),
                LawTesterStage(values=(-1e-1,0,0,0,0,0),whats='v.....',until='C.geom.uN<%g'%(-r*2e-2)),
                LawTesterStage(values=(1e-1,0,0,0,0,0),whats='v.....',until='C.geom.uN>%g'%(-r*1e-4)),
                # shear while not moving in the normal sense
                #LawTesterStage(values=(0,1e-2,0,0,0,0),whats='vv....',until='C.phys.xiT.norm()>1e-3'),
                # unload in the normal sense
                # LawTesterStage(values=(1e-3,0,0,0,0,0),whats='v.....',until='not C'),
            ],
            done='S.stop()',label='tester'
        ),
        PyRunner(5,'C=S.dem.con[0]; S.plot.addData(i=S.step,time=S.time,f=C.phys.force,deltaMax=C.phys.deltaMax,ftNorm=C.phys.force.yz().norm(),t=C.phys.torque,uN=C.geom.uN,normBranch=C.phys.normBranch)')
    ],
    plot=Plot(plots={'uN':('f_x'),'uN ':('deltaMax'),' uN':('normBranch')}) # ,'f_x':('ftNorm',)}
)

S.saveTmp()
S.run()
#O.reload()
