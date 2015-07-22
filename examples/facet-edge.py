import woo, woo.dem, woo.core, woo.triangulated
from minieigen import *

# video of this simulation at https://youtu.be/PkU4PLrCgF0

S=woo.master.scene=woo.core.Scene(
	fields=[woo.dem.DemField(gravity=(5,0,-10))], # gravity sideways
	# add significant damping to avoid oscillations
	engines=woo.dem.DemField.minimalEngines(damping=.6)+[woo.dem.Tracer(num=10000,scalar='numcon',stepPeriod=1)],
	dtSafety=.7,
	throttle=.01, # slow execution
	stopAtTime=1.15,
)
# very soft material
m=woo.dem.FrictMat(young=1e6,density=2000) 
# sphere plus triangulated band
S.dem.par.add([woo.dem.Sphere.make((.1,0,.05),radius=.05,mat=m)]+woo.triangulated.quadrilateral(Vector3(0,-.05,0),Vector3(0,.05,0),Vector3(1,-.05,0),Vector3(1,.05,0),mat=m,div=(1,10)))
S.saveTmp()

try:
	import woo.gl, woo.qt
	woo.gl.Renderer(dispScale=(1,1,1000),scaleOn=True,iniViewDir=(0,1,0)) # scale vertical displacement 
	woo.gl.Gl1_DemField.glyph='force'
	woo.qt.View()
except ImportError: pass
