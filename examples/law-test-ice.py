import woo
from woo import utils
from woo.core import *
from woo.dem import *
from woo import plot
from woo import *
import woo.log
#woo.log.setLevel('LawTester',woo.log.INFO)
#woo.log.setLevel('Law2_L6Geom_PelletPhys_Pellet',woo.log.TRACE)
m=IceMat(density=1e3,young=1e7,ktDivKn=.2,tanPhi=.5)
S=woo.master.scene=Scene(fields=[DemField(gravity=(0,0,0))])
S.dem.par.add([Sphere.make((0,0,0),.5,fixed=False,wire=True,mat=m),Sphere.make((0,.9999999,0),.5,fixed=False,wire=True,mat=m)])
S.dem.collectNodes()
S.engines=DemField.minimalEngines(damping=.0,cp2=Cp2_IceMat_IcePhys(bonds0=255,bonds1=255),law=Law2_L6Geom_IcePhys())+[
	LawTester(ids=(0,1),abWeight=.3,smooth=1e-4,stages=[
			LawTesterStage(values=(-.01,0,0,0,0,0),whats='v.....',until='C and C.geom.uN<-1e-3',done='print "Compressed to",C.geom.uN'),
			LawTesterStage(values=(.01,0,0,0,0,0),whats='v.....',until=('not C'),done='print "Unloaded to breakage"'),
		],
		done='tester.dead=True; S.stop(); print "Everything done, making myself dead and pausing."',
		label='tester'
	),
	PyRunner(1,'import woo; dd={}; dd.update(**S.lab.tester.fuv()); dd.update(**S.energy); S.plot.addData(i=S.step,dist=(S.dem.par[0].pos-S.dem.par[1].pos).norm(),uNPl=(S.dem.con[0].data.uNPl if S.dem.con[0].data else float("nan")),t=S.time,**dd)'),
]
S.dt=1e-3
#S.pause()
S.trackEnergy=True
#plot.plots={' i':(('fErrRel_xx','k'),None,'fErrAbs_xx'),'i ':('dist',None),' i ':(**S.energy),'   i':('f_xx',None,'f_yy','f_zz'),'  i':('u_xx',None,'u_yy','u_zz'),'i  ':('u_yz',None,'u_zx','u_xy')}
S.plot.plots={'i':('u_xx',None,'uNPl'),'u_xx':('f_xx',)}
S.plot.plot()
S.saveTmp()
S.run()
#O.reload()
