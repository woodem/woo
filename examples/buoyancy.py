import woo, woo.dem, woo.core, woo.models
from woo.dem import *

mat=FrictMat(young=1e5,density=1e3,tanPhi=.5)
S=woo.master.scene=woo.core.Scene(
    dtSafety=.2, # small so that plots are smoother
    stopAtTime=11,
    throttle=.03, # pause between steps to limit execution speed
    plot=woo.core.Plot(plots={'t':('p_z',None,('v_z','r')),'t ':('F_z','Fg_only')}),
    engines=DemField.minimalEngines(model=woo.models.ContactModelSelector(name='linear',damping=0.0))+[
        HalfspaceBuoyancy(liqRho=mat.density,surfPt=(0,0,5),dragCoef=0.9,drag=True),
        woo.core.PyRunner(1,command='addPlotData()'),
    ],
    fields=[DemField(gravity=(0,0,-9.81),
        par=Wall.makeBox(box=((0,0,0),(4,4,4)),which=(1,1,1,1,1,0),mat=mat)+[
            Sphere.make((2,2,6),radius=1,mat=mat)
        ]
    )],
)
S.saveTmp()

def addPlotData():
    s=S.dem.par[-1] # sphere
    S.plot.addData(p=s.pos,v=s.vel,F=s.f,Fg_only=-s.mass*S.dem.gravity.norm(),t=S.time)
