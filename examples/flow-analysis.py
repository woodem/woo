from minieigen import *
from woo.dem import *
from woo.core import *
import woo, woo.triangulated
m=FrictMat(young=10e6)
domain=AlignedBox3((-.4,-.1,-.5),(.4,.4,.3))
S=woo.master.scene=Scene(
	fields=[DemField(gravity=(0,0,-9.81),par=woo.triangulated.quadrilateral(Vector3(-.4,-.1,0),Vector3(-.4,.4,0),Vector3(.3,0,-.5),Vector3(.3,.3,-.2),div=(5,5),mat=m,wire=False))],
	engines=DemField.minimalEngines(dynDtPeriod=100)+[
		BoxOutlet(stepPeriod=100,box=domain,inside=False),
		BoxInlet(
			stepPeriod=100,box=((-.3,0,0),(0,.3,.3)),label='inA',
			generator=PsdSphereGenerator(psdPts=[(.01,0),(.02,1)]),
			massRate=3,
			materials=[m],
		),
		BoxInlet(
			stepPeriod=100,box=((0,0,0),(.3,.3,.3)),label='inB',
			generator=PsdSphereGenerator(psdPts=[(.02,0),(.03,1)]),
			massRate=3,
			materials=[m],
		),
		VtkExport(stepPeriod=1,nDo=1,out='/tmp/{id}',mkDir=True), # to export mesh, which is visible in flow analysis 
		FlowAnalysis(stepPeriod=50,box=domain,dLim=[.02],cellSize=.01),
		PyRunner(20000,initRun=False,nDo=1,command='import woo.paraviewscript\nwoo.paraviewscript.fromFlowAnalysis(S.engines[-2],launch=True); S.stop()')
	]
)

import woo.qt
woo.qt.View()

S.run()


