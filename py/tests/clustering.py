'''
Tests for the new :obj:`woo.dem.ClusterAnalysis`.
'''
from builtins import range
import unittest
import woo, woo.dem, woo.core, woo.utils
from math import *
from minieigen import *


class TestClusterAnalysis(unittest.TestCase):
    def testZeroConnClustering(self):
        'clustering: simple zero-connectivity clustering'
        S=woo.core.Scene(fields=[woo.dem.DemField(par=[woo.dem.Sphere.make(c,.3) for c in [(0,0,0),(1,0,0),(1,.5,0),(2,0,0),(2,.5,0),(2.25,.25,0),(3,0,0),(3,.5,0),(3.25,.25,0),(3.125,.25,.25)]])],engines=woo.dem.DemField.minimalEngines()+[woo.dem.ClusterAnalysis(stepPeriod=1,clustMin=0)])
        S.one()
        # all spheres in one group should have the same ID
        grps=[[0],[1,2],[3,4,5],[6,7,8,9]]
        #for grp in grps:
        #    for g in grp:
        #        print(str(grp),g,S.dem.par[g].matState.labels[0])
        labels=[]
        for grp in grps:
            self.assertEqual(len(set([S.dem.par[i].matState.labels[0] for i in grp])),1)
            labels.append(S.dem.par[grp[0]].matState.labels[0])
        self.assertEqual([0,1,2,3],sorted(labels))



