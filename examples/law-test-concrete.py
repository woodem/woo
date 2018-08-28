import woo, woo.plot, woo.utils
from woo import utils
from woo.core import *
from woo.dem import *
from woo import *
import woo.log
r=1e-3
m=ConcreteMat(young=30e9,ktDivKn=.2,sigmaT=3e6,tanPhi=.8,epsCrackOnset=1e-4,relDuctility=5.)
S=woo.master.scene=Scene(
   fields=[DemField(
        distFactor=1.5,
        parNodes=True, # add nodes of those particles, even if they are fixed
        par=[Sphere.make((0,0,0),r,fixed=True,wire=True,mat=m),Sphere.make((0,2*r,0),r,fixed=True,wire=True,mat=m)])
    ],
   engines=DemField.minimalEngines(model=woo.models.ContactModelSelector(name='concrete'),lawKw=dict(yieldSurfType='loglin'))+[
        LawTester(
            ids=(0,1),abWeight=.5,smooth=1e-4,stages=[
                # compress
                LawTesterStage(values=(-1e-4,0,0,0,0,0),whats='v.....',until='C.phys.sigmaN<-2e7'),
                # shear while not moving in the normal sense
                LawTesterStage(values=(0,1e-3,0,0,0,0),whats='vv....',until='C.phys.epsT.norm()>1e-2'),
                # unload in the normal sense; shear force should follow the plastic surface
                LawTesterStage(values=(1e-4,0,0,0,0,0),whats='v.....',until='not C'),
            ],
            done='S.stop()',label='tester'
        ),
        PyRunner(5,'C=S.dem.con[0]; S.plot.addData(i=S.step,**C.phys.dict())')
    ],
   plot=Plot(plots={'epsN':('sigmaN',),'sigmaN':('sigmaT_norm',)}
        #,labels={'epsN':r'$\varepsilon_N$','sigmaN':r'$\sigma_N$'}
    )
)
S.saveTmp()
S.run()
#O.reload()
