#!/usr/bin/python
# -*- coding: utf-8 -*-
"""This example demonstrates GTS (http://gts.sourceforge.net/) opportunities for creating surfaces
VTU-files are created in /tmp directory after simulation. If you open those with paraview
(or other VTK-based) program, you can create video, make screenshots etc."""
import woo
from woo import *
from woo.dem import *
from woo.core import *
from numpy import linspace
from woo import pack,qt,utils
from minieigen import *
from math import *

S=woo.master.scene=Scene(fields=[DemField(gravity=(0,0,-9.81))])

thetas=linspace(0,2*pi,num=16,endpoint=True)
meridians=pack.revolutionSurfaceMeridians([[Vector2(3+rad*sin(th),10*rad+rad*cos(th)) for th in thetas] for rad in linspace(1,2,num=10)],angles=linspace(0,pi,num=10))
surf=pack.sweptPolylines2gtsSurface(meridians+[[Vector3(5*sin(-th),-10+5*cos(-th),30) for th in thetas]])
S.dem.par.add(pack.gtsSurface2Facets(surf))
S.dem.par.add(InfCylinder.make((0,0,0),axis=0,radius=2,glAB=(-10,10)))
# edgy helix from contiguous rods
nPrev=None
for i in range(25):
    nNext=Node(pos=(3+3*sin(i),3*cos(i),3+.3*i))
    if nPrev: S.dem.par.add(Rod.make(vertices=[nPrev,nNext],radius=.5,wire=False,fixed=True))
    nPrev=nNext

sp=pack.SpherePack()
sp.makeCloud(Vector3(-1,-9,30),Vector3(1,-13,32),.2,rRelFuzz=.3)
S.dem.par.add([utils.sphere(c,r) for c,r in sp])

S.engines=DemField.minimalEngines(damping=.2)+[VtkExport(stepPeriod=100,what=VtkExport.spheres|VtkExport.mesh,out='/tmp/p1-'),PyRunner(initRun=False,stepPeriod=0,virtPeriod=13,nDo=1,command='import woo.paraviewscript; woo.paraviewscript.fromEngines(S,launch=True); S.stop()')]

qt.Controller()
qt.View()
S.saveTmp()

