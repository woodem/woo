from woo.core import *
from woo.dem import *
from woo import timing
import woo, woo.utils, woo.log
import sys
woo.log.setLevel('AabbTreeCollider',woo.log.INFO)

woo.master.usesApi=10101

S=woo.master.scene
S.throttle=.05
S.fields=[DemField(gravity=(-5,-5,-5))]
S.lab.spheMask=0b001
S.lab.wallMask=0b011
S.lab.loneMask=0b010
S.dem.loneMask=S.lab.loneMask
woo.master.timingEnabled=True

rr=.05
#rr=.1
# cell=rr
verlet=.1*rr
verletSteps=0
mat=woo.utils.defaultMaterial()

treeCollider=AabbTreeCollider([Bo1_Sphere_Aabb(),Bo1_Wall_Aabb()],label='collider',verletDist=verlet)
sortCollider=InsertionSortCollider([Bo1_Sphere_Aabb(),Bo1_Wall_Aabb()],verletDist=verlet,label='collider')


if 0:
    S.engines=[
        Leapfrog(damping=0.3,reset=True),
        (sortCollider if 'sort' in sys.argv else treeCollider),
        ContactLoop(
            [Cg2_Sphere_Sphere_L6Geom(),Cg2_Facet_Sphere_L6Geom(),Cg2_Wall_Sphere_L6Geom(),Cg2_InfCylinder_Sphere_L6Geom()],
            [Cp2_FrictMat_FrictPhys()],
            [Law2_L6Geom_FrictPhys_IdealElPl(noSlip=True)],
            applyForces=True,
            label='contactLoop',
        ),
    ]
else:
    S.engines=woo.dem.minimalEngines(collider=('sap' if 'sort' in sys.argv else 'tree'))+[
        PyRunner(1000,'import woo.timing; print("step %d, %d contacts, %g%% real"%(S.step,len(S.dem.con),S.dem.con.realRatio()*100)); woo.timing.stats(); woo.timing.reset(); S.lab.collider.nFullRuns=0;'),
    ]

if 0:
    for c,r in [((0,0,.4),0.2),((1,1,1),.3),((1.3,2,2),.4),((.5,.5,.5),.5)][:-1]:
        S.dem.par.add(Sphere.make(c,2.1*r,mask=S.lab.spheMask))
    # S.lab.collider.renderCells=True
else:
    import woo.pack
    sp=woo.pack.SpherePack()
    print('Making cloud...')
    sp.makeCloud((.1,.1,.0),(1.1,1.1,1.0),rMean=rr,rRelFuzz=.3)
    print('Generated cloud with %d spheres'%(len(sp)))
    sp.toSimulation(S,mat=mat,mask=S.lab.spheMask)
    print('Spheres added to Scene')

S.dem.par.add([
    Wall.make(0,axis=2,sense=1,mat=mat,mask=S.lab.wallMask),
    Wall.make(-.5,axis=0,sense=1,mat=mat,mask=S.lab.wallMask),
    Wall.make(-.5,axis=1,sense=1,mat=mat,mask=S.lab.wallMask),
],nodes=False)

S.saveTmp()
#S.run(3001,wait=True)
S.one()
print(len(S.dem.con))
