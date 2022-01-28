# encoding: utf-8
# 2013 © Václav Šmilauer <eu@doxos.eu>

import unittest
from minieigen import *
import woo.core
import woo.dem
import woo.utils
import math
from woo.dem import *

class TestShapePack(unittest.TestCase):
    # def setUp(self):
    def testLoadSampleTxt(self):
        'ShapePack: load sample text file'
        data='''##PERIODIC:: 1. 1. 1.
0 Sphere 0 0 0 .1
1 Sphere .1 .1 .1 .1
1 Sphere .1 .1 .2 .1
2 Sphere .2 .2 .2 .2
2 Sphere .3 .3 .3 .3
2 Capsule .4 .4 .4 .3 .05 .05 .05
'''
        tmp=woo.master.tmpFilename()
        f=open(tmp,'w'); f.write(data); f.close()
        sp=ShapePack(loadFrom=tmp)
        self.assertTrue(len(sp.raws)==3)
        self.assertTrue(type(sp.raws[1])==SphereClumpGeom) # automatic conversion for sphere-only clumps
        self.assertTrue(sp.raws[2].rawShapes[2].className=='Capsule')
        self.assertTrue(sp.cellSize[0]==1.)
        # print sp.raws
    def testSingle(self):
        'ShapePack: single particles not clumped when inserted into simulation'
        S=woo.core.Scene(fields=[DemField()])
        sp=ShapePack(raws=[SphereClumpGeom(centers=[(1,1,1)],radii=[.1]),RawShapeClump(rawShapes=[RawShape(className='Sphere',center=(2,2,2),radius=.2,raw=[])])])
        mat=woo.utils.defaultMaterial()
        sp.toDem(S,S.dem,mat=mat)
        self.assertTrue(len(S.dem.nodes)==2)
        self.assertTrue(S.dem.par[0].pos==(1,1,1))
        self.assertTrue(S.dem.par[1].shape.radius==.2)
        self.assertTrue(not S.dem.par[0].shape.nodes[0].dem.clumped)
        self.assertTrue(not S.dem.par[1].shape.nodes[0].dem.clumped)
    def testFromSim(self):
        'ShapePack: from/to simulation with particles'
        S=woo.core.Scene(fields=[DemField()])
        # add two clumped spheres first
        r1,r2,p0,p1=1,.5,Vector3.Zero,Vector3(0,0,3)
        # adds clump node to S.dem.nodes automatically
        S.dem.par.addClumped([
            woo.utils.sphere((0,0,0),1),
            woo.utils.sphere((0,0,3),.5)
        ])
        # add a capsule
        c=woo.utils.capsule(center=(5,5,5),shaft=.3,radius=.3)
        S.dem.par.add(c,nodes=False)
        S.dem.nodesAppend(c.shape.nodes[0])
        # from DEM
        sp=ShapePack()
        sp.fromDem(S,S.dem,mask=0)
        self.assertTrue(len(sp.raws)==2)
        self.assertTrue(type(sp.raws[0])==SphereClumpGeom)
        self.assertTrue(sp.raws[1].rawShapes[0].className=='Capsule')
        #print sp.raws
        # to DEM
        mat=woo.utils.defaultMaterial()
        S2=woo.core.Scene(fields=[DemField()])
        sp.cellSize=(.2,.2,.2)
        sp.toDem(S2,S2.dem,mat=mat)
        # test that periodicity is used
        self.assertTrue(S2.periodic==True)
        self.assertTrue(S2.cell.size[0]==.2)
        # for p in S2.dem.par: print p, p.shape, p.shape.nodes[0]
        # for n in S2.dem.nodes: print n
        self.assertTrue(len(S2.dem.par)==3) # two spheres and capsule
        self.assertTrue(S2.dem.par[0].shape.nodes[0].dem.clumped) # sphere node is in clump
        self.assertTrue(S2.dem.par[0].shape.nodes[0] not in S2.dem.nodes) # sphere node not in dem.nodes
        self.assertTrue(S2.dem.nodes[0].dem.clump) # two-sphere clump node
        self.assertTrue(not S2.dem.nodes[1].dem.clump) # capsule node
        # TODO: test that particle positions are as they should be
    def assertAlmostEqualRel(self,a,b,relerr,abserr=0):
        self.assertAlmostEqual(a,b,delta=max(max(abs(a),abs(b))*relerr,abserr))
    def testGridSamping(self):
        'ShapePack: grid samping gives good approximations of mass+inertia'
        # take a single shape, compare with clump of zero-sized sphere (to force grid samping) and that shape
        m=woo.utils.defaultMaterial()
        zeroSphere=woo.utils.sphere((0,0,0),.4) # sphere which is entirely inside the thing
        for p in [woo.utils.sphere((0,0,0),1,mat=m),woo.utils.ellipsoid((0,0,0),semiAxes=(.8,1,1.2),mat=m),woo.utils.capsule((0,0,0),radius=.8,shaft=.6,mat=m)]:
            # print p.shape
            sp=woo.dem.ShapePack()
            sp.add([p.shape,zeroSphere.shape])
            r=sp.raws[0]
            r.recompute(div=10)
            # this depends on how we define equivalent radius, which is not clear yet; so just skip it
            # self.assertAlmostEqualRel(r.equivRad,p.shape.equivRadius,1e-2)
            self.assertAlmostEqualRel(r.volume,p.mass/m.density,1e-2)
            # sorted since axes may be swapped
            ii1,ii2=sorted(r.inertia),sorted(p.inertia/m.density)
            for i1,i2 in zip(ii1,ii2): self.assertAlmostEqualRel(i1,i2,1e-2)
            for ax in (0,1,2): self.assertAlmostEqualRel(r.pos[ax],p.pos[ax],0,1e-2)
    def testLineEndings(self):
        'ShapePack: handle different line endings (LF, CR+LF, CR) gracefully'
        data='0 Sphere 0 0 0 .1|1 Sphere .1 .1 .1 .1|1 Sphere .1 .1 .2 .1|2 Sphere .2 .2 .2 .2|'
        for le,leName in [('\n','LF'),('\r\n','CR+LF'),('\r','CR')]:
            d2=data.replace('|',le)
            tmp=woo.master.tmpFilename()
            f=open(tmp,'w'); f.write(d2); f.close()
            try: sp=ShapePack(loadFrom=tmp)
            except: self.fail("Loading text file with line endings %s failed.")
            self.assertEqual(len(sp.raws),3)
