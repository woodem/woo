from woo.core import *
from woo.dem import *
from minieigen import *
import woo.models, woo

woo.master.usesApi=10102

S=woo.master.scene=Scene(fields=[DemField(gravity=(0,0,-10))])

# contact model plus material for particles
model=woo.models.ContactModelSelector(name='linear',damping=.4,mats=[FrictMat(young=1e7,ktDivKn=.2,tanPhi=.4)])
# same material for walls as for particles, but without friction
S.lab.wallMat=model.mats[0].deepcopy(tanPhi=0.)
# intial box size
box=AlignedBox3((0,0,0),(2,2,2))
# use something else than spheres here, if you like
gen=PsdSphereGenerator(psdPts=[(.1,0),(.1,.2),(.2,.9),(.3,1)])
ctrlStep=100

# wall box without the top (will be added later)
S.dem.par.add(Wall.makeBox(box=box,which=(1,1,1,1,1,0),mat=S.lab.wallMat))

S.engines=DemField.minimalEngines(model=model)+[
    # generate initial particles, until massRate cannot be achieved, then turn yourself off (dead)
    BoxInlet(stepPeriod=ctrlStep,box=box,generator=gen,materials=[model.mats[0]],massRate=1000,maxMass=0,maxAttempts=1000,atMaxAttempts='dead',label='inlet'),
    # check what's happening periodically
    PyRunner(ctrlStep,'checkProgress(S)')
]

# stage: 0=gravity deposition, 1=oedometric loading, 2=oedometric unloading, 3=everything finished
S.lab.stage=0
# avoid warning when we change the value
S.lab._setWritable('stage')
# things that we want to look at during the simualtion
S.plot.plots={'dz':('Fz',)}

S.saveTmp()

S.run()

# callback function from PyRunner
def checkProgress(S):
    # if the top wall is there, save its position and acting force
    if S.lab.stage>0: S.plot.addData(dz=S.lab.z0-S.lab.topWall.pos[2],Fz=S.lab.topWall.f[2],i=S.step,t=S.time)
    # check progress
    # if inlet is off and unbalanced force is already low, go ahead
    if S.lab.stage==0 and S.lab.inlet.dead and woo.utils.unbalancedForce(S)<0.05:
        S.lab.stage+=1
        # find the topmost particle (only spheroids (i.e. Spheres, Ellipsoids, Capsules) have equivRadius)
        z0=max([p.pos[2]+p.shape.equivRadius for p in S.dem.par if p.shape.equivRadius>0])
        # place Wall just above the top
        S.lab.topWall=Wall.make(z0,axis=2,sense=-1,mat=S.lab.wallMat)
        # save the initial z as reference
        S.lab.z0=z0
        # assign constant velocity to the wall
        S.lab.topWall.vel=(0,0,-.05)
        # add to the simulation
        S.dem.par.add(S.lab.topWall)
    # when the wall goes down to z=1, make it move upwards again
    elif S.lab.stage==1 and S.lab.topWall.pos[2]<1:
        S.lab.stage+=1
        # reverse the loading sense
        S.lab.topWall.vel*=-1 
    # move until the initial position is reached again
    elif S.lab.stage==2 and S.lab.topWall.pos[2]>S.lab.z0:
        # everything done, nothing will happen with stage==3
        S.lab.stage+=1
        # stop the wall
        S.lab.topWall.vel=(0,0,0) 
        # and stop the simulation as well
        S.stop()
