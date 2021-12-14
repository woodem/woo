from woo.dem import *
from woo.core import *
import woo
r=1e-3
m=ConcreteMat(young=30e9,ktDivKn=.2,sigmaT=3e6,tanPhi=.8,epsCrackOnset=1e-4,relDuctility=5.)
S=woo.master.scene=Scene(
   fields=[DemField(par=[Sphere.make((0,0,0),r,fixed=False,wire=True,mat=m),Sphere.make((0,2*r,0),r,fixed=False,wire=True,mat=m)])],
   engines=DemField.minimalEngines(model=woo.models.ContactModelSelector(name='concrete',distFactor=1.5),lawKw=dict(epsSoft=-2*m.epsCrackOnset,relKnSoft=.5))+[LawTester(ids=(0,1),abWeight=.5,smooth=1e-4,stages=[LawTesterStage(values=((1 if i%2==0 else -1)*1e-2,0,0,0,0,0),whats='v.....',until='C.phys.epsN'+('>' if epsN>0 else '<')+'%g'%epsN) for i,epsN in enumerate([6e-4,-6e-4,12e-4,-8e-4,15e-4])],done='S.stop()',label='tester'),PyRunner(5,'C=S.dem.con[0]; S.plot.addData(i=S.step,**C.phys.dict())')],
   plot=Plot(plots={'epsN':('sigmaN',)},labels={'epsN':r'$\varepsilon_N$','sigmaN':r'$\sigma_N$'}) # ,'i':('epsN',None,'omega')}
)
S.run(); S.wait()
S.plot.plot()