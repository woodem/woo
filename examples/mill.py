#!/usr/bin/python
# -*- coding: utf-8 -*-
"""
Small showcase posted at http://www.youtube.com/watch?v=KUv26xlh89I,
in response to pfc3d's http://www.youtube.com/watch?v=005rdDBoe4w.
Physical correctness is not the focus, the geometry and similar look is.

You can take this file as instruction on how to build parametric surfaces,
and how to make videos as well.
"""
import woo
import woo.core, woo.utils, woo.pack, woo.dem, woo.pack
from minieigen import *
from numpy import linspace
from math import *
# geometry parameters
bumpNum=20
bumpHt,bumpTipAngle=0.07,60*pi/180
millRad,millDp=1,1 # radius and depth (cylinder length) of the mill
sphRad,sphRadFuzz=0.03,.8 # mean radius and relative fuzz of the radius (random, uniformly distributed between sphRad*(1-.5*sphRadFuzz)…sphRad*(1+.5*sphRadFuzz))
dTheta=pi/24 # circle division angle


S=woo.master.scene=woo.core.Scene(fields=[woo.dem.DemField(gravity=(0,0,-10))])

# define domains for initial cloud of red and blue spheres
packHt=.8*millRad # size of the area
bboxes=[(Vector3(-.5*millDp,-.5*packHt,-.5*packHt),Vector3(.5*millDp,0,.5*packHt)),(Vector3(-.5*millDp,0,-.5*packHt),Vector3(.5*millDp,.5*packHt,.5*packHt))]
colors=.2,.7
for i in (0,1): # red and blue spheres
    sp=woo.pack.SpherePack(); bb=bboxes[i]; vol=(bb[1][0]-bb[0][0])*(bb[1][1]-bb[0][1])*(bb[1][2]-bb[0][2])
    sp.makeCloud(bb[0],bb[1],sphRad,sphRadFuzz)
    S.dem.par.add([woo.dem.Sphere.make(s[0],s[1],color=colors[i]) for s in sp])

###
### mill geometry (parameteric)
###
millPar,centralNode=woo.triangulated.ribbedMill((-.5*millDp,0,0),(.5*millDp,0,0),radius=millRad,majNum=bumpNum,majHt=bumpHt,majTipAngle=bumpTipAngle,div=24,wire=False,color=.5)
millPar+=[woo.dem.Wall.make(-.5*millDp,axis=0,sense=+1),woo.dem.Wall.make(.5*millDp,axis=0,sense=-1)]
for w in millPar[-2:]: w.shape.visible=False
S.dem.par.addClumped(millPar,centralNode=centralNode)
centralNode.dem.angVel=(-5,0,0)


#print "Numer of grains",len(O.dem.par)-len(millIds)

# S.dt=.9*woo.utils.pWaveDt()
S.engines=woo.utils.defaultEngines(damping=.3)+[
    woo.core.PyRunner(10,'S.plot.addData(i=S.step,dt=S.dt,dynDt=S.lab.dyndt.dt)'),woo.dem.DynDt(stepPeriod=200,dryRun=False,label='dyndt',maxRelInc=1e-4)
]

S.plot.plots={'i':('dt','dynDt')}

S.saveTmp()
try:
    from woo import qt
    v=qt.View()
    v.eyePosition=(3,.8,.96); v.upVector=(-.4,-.4,.8); v.viewDir=(-.9,-.25,-.3); v.axes=True; v.sceneRadius=1.9
except ImportError: pass
#O.run(20000); O.wait()
#utils.encodeVideoFromFrames(snapshooter['savedSnapshots'],out='/tmp/mill.ogg',fps=30)
