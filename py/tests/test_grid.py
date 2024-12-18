# encoding: utf-8
# 2013 © Václav Šmilauer <eu@doxos.eu>
import woo
import unittest
from minieigen import *
import woo.dem


class TestGridStore(unittest.TestCase):
    def setUp(self):
        self.gs=woo.dem.GridStore(gridSize=(5,6,7),cellLen=4,denseLock=True,exNumMaps=4)
        self.ijk=Vector3i(2,3,4)
    def testEx(self):    
        'Grid: storage: dense and extra data'
        for i in range(0,10): self.gs.append(self.ijk,i)
        self.assertTrue(self.gs[self.ijk]==list(range(0,10)))
        dense,extra=self.gs._rawData(self.ijk)
        # print self.gs[self.ijk],dense,extra
        self.assertTrue(dense==[10,0,1,2,3])
        self.assertTrue(extra[:6]==[4,5,6,7,8,9])
        self.assertTrue(self.gs.countEx()=={tuple(self.ijk):6})
    def testAppend(self):
        'Grid: storage: appending data'
        for i in range(0,13):
            self.gs.append(self.ijk,i)
            self.assertTrue(i==self.gs[self.ijk][self.gs.size(self.ijk)-1])
    def testStorageOrder(self):
        'Grid: storage: storage order'
        self.assertTrue(self.gs.lin2ijk(1)==(0,0,1)) # last varies the fastest
        self.assertTrue(self.gs.ijk2lin((0,0,1))==1)
    def testPyAcces(self):
        'Grid: storage: python access'
        self.gs[self.ijk]=range(0,10)
        self.assertTrue(self.gs[self.ijk]==list(range(0,10)))
        self.assertTrue(self.gs.countEx()=={tuple(self.ijk):6})
        del self.gs[self.ijk]
        self.assertTrue(self.gs.countEx()=={})
        self.assertTrue(self.gs.size(self.ijk)==0)
        self.assertTrue(self.gs[self.ijk]==[])
    def testComplement(self):
        'Grid: storage: complements'
        # make insignificant parameters different
        g1=woo.dem.GridStore(gridSize=(3,3,3),cellLen=2,denseLock=False,exNumMaps=4)
        g2=woo.dem.GridStore(gridSize=(3,3,3),cellLen=3,denseLock=True,exNumMaps=2)
        c1,c2,c3,c4=(1,1,1),(2,2,2),(2,1,2),(1,2,1)
        g1[c1]=[0,1]; g2[c1]=[1,2] # mixed scenario
        g1[c2]=[1,2,3]; g2[c2]=[]  # b is empty (cheaper copy branch)
        g2[c3]=[]; g2[c3]=[1,2,3]  # a is empty (cheaper copy branch)
        g2[c4]=[]; g2[c4]=[]
        # incompatible dimensions
        self.assertRaises(RuntimeError,lambda: g1.complements(woo.dem.GridStore(gridSize=(2,2,2))))
        # setMinSize determines when is boost::range::set_difference and when naive traversal used (presumably faster for very small sets)
        for setMinSize in (0,1,2,3):
            g12,g21=g1.complements(g2,setMinSize=setMinSize)
            if 0:
                print(setMinSize,'g1',g1[c1],g1[c2],g1[c3],g1[c4])
                print(setMinSize,'g2',g2[c1],g2[c2],g2[c3],g2[c4])
                print(setMinSize,'g12',g12[c1],g12[c2],g12[c3],g12[c4])
                print(setMinSize,'g21',g21[c1],g21[c2],g21[c3],g12[c4])
            self.assertTrue(g12[c1]==[0])
            self.assertTrue(g21[c1]==[2])
            self.assertTrue(g12[c2]==[1,2,3])
            self.assertTrue(g21[c2]==[])
            self.assertTrue(g12[c3]==[])
            self.assertTrue(g21[c3]==[1,2,3])
            self.assertTrue(g21[c4]==[])
            self.assertTrue(g12[c4]==[])


class TestGridColliderBasics(unittest.TestCase):
    def testParams(self):
        'GridCollider: used-definable parameters'
        gc=woo.dem.GridCollider()
        gc.domain=((0,0,0),(1,1,1))
        gc.minCellSize=.1
        self.assertTrue(gc.dim==Vector3i(10,10,10))
        self.assertAlmostEqual(gc.cellSize[0],.1)
        self.assertRaises(RuntimeError,lambda: setattr(gc,'minCellSize',0))
        gc.minCellSize=.1
        self.assertRaises(RuntimeError,lambda: setattr(gc,'domain',((0,0,0),(0,0,0))))
        self.assertRaises(RuntimeError,lambda: setattr(gc,'domain',((0,0,0),(-1,-1,-1))))
        
