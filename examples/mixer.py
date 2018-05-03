# this exmaple shows how to get torque from a clump,
# which is a bit tricky in this case, as contact forces
# must be transferred to nodes first (facets have 3 nodes
# and therefore the transfer does not happen automatically,
# unlike with spheres, for instance)
import woo, woo.dem, woo.utils
from minieigen import *
# create scene
S=woo.master.scene=woo.core.Scene(fields=[woo.dem.DemField(gravity=(0,0,-10))],dtSafety=.9)
# add cylinder around
S.dem.par.add(woo.triangulated.cylinder(Vector3(0,0,0),Vector3(0,.2,0),radius=.05,capA=True,capB=True,wallCaps=True,fixed=True))
# mixer node (handle) in the origin
S.lab.mixNode=woo.core.Node(pos=(0,0,0))
# add two blades as clump with that handle
S.dem.par.addClumped([woo.dem.Facet.make(((-.05,0,0),(-.05,.2,0),(.05,.2,0)),wire=False),woo.dem.Facet.make(((0,.2,.05),(0,0,.05),(0,0,-.05)),wire=False)],centralNode=S.lab.mixNode)
# no movement due to contact forces
S.lab.mixNode.dem.blocked='xyzXYZ'
# prescribe angular velocity
S.lab.mixNode.dem.angVel=(0,10,0)
# standard engines
S.engines=woo.dem.DemField.minimalEngines()+[
    # run only in the first step, to generate some particles
    woo.dem.BoxInlet(box=((0,0,0),(.035,.2,.035)),
        generator=woo.dem.PsdSphereGenerator(psdPts=((.004,0),(.008,1))),
        maxMass=-1,maxNum=-1,massRate=1e10,maxAttempts=100,atMaxAttempts='dead',
        materials=[S.dem.par[0].mat],
    ),
    # necessary to transfer force from contact points to nodes
    # (this is done automatically one for mono-nodal particles like spheres)
    woo.dem.IntraForce([woo.dem.In2_Facet()]),
    # get summary torque in the y direction on the blades
    woo.core.PyRunner(50,'S.plot.autoData()')
]
# shorthand plotting function, gets values by evaluating the expression after =
S.plot.plots={'t=S.time':('T=woo.dem.ClumpData.forceTorqueFromMembers(S.lab.mixNode)[1][1]',)}
S.saveTmp()
