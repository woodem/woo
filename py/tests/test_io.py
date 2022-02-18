'''
Test loading and saving woo objects in various formats
'''
import woo
import unittest
from woo.core import *
from woo.dem import *
from woo.pre import *
from minieigen import *
from woo import utils

class TestFormatsAndDetection(unittest.TestCase):
    def setUp(self):
        woo.master.scene=S=Scene(fields=[DemField()])
        S.engines=utils.defaultEngines()
        S.dem.par.add(utils.sphere((0,0,0),radius=1))
    def tryDumpLoad(self,fmt='auto',ext='',load=True):
        S=woo.master.scene
        for o in S.engines+[S.dem.par[0]]:
            out=woo.master.tmpFilename()+ext
            o.dump(out,format=fmt)
            if load:
                o2=Object.load(out,format='auto')
                self.assertTrue(type(o2)==type(o))
                o3=type(o).load(out,format='auto')
                self.assertRaises(TypeError,lambda: woo.core.Node.load(out))
                #if fmt=='expr': print open(out).read()
    def tryDumpLoadStr(self,fmt,load=True):
        S=woo.master.scene
        for o in S.engines+[S.dem.par[0]]:
            dump=o.dumps(format=fmt)
            if load: Object.loads(dump,format='auto')
    def testRefuseDerivedPyObject(self):
        'IO: python-derived objects refuse to save via boost::serialization.'
        import woo.pre.horse
        fh=woo.pre.horse.FallingHorse()
        out=woo.master.tmpFilename()+'.bin.gz'
        self.assertRaises(IOError,lambda: fh.dump(out)) # this should deted boost::serialization anyway
        self.assertRaises(IOError,lambda: fh.dump(out,format='boost::serialization'))
    def testStrFile(self):
        'IO: file can be given as str'
        out=woo.master.tmpFilename()+'.expr'
        woo.master.scene.dem.par[0].dump(out,format='expr')
    def testUnicodeFile(self):
        'IO: filename can be given as unicode'
        out=str(woo.master.tmpFilename()+'.expr')
        woo.master.scene.dem.par[0].dump(out,format='expr')
    def testExpr(self):
        'IO: expression dump/load & format detection (file+string)'
        self.tryDumpLoad(fmt='expr')
        self.tryDumpLoadStr(fmt='expr')
    def testJson(self):
        'IO: JSON dump/load & format detection (file+string)'
        self.tryDumpLoad(fmt='json')
        self.tryDumpLoadStr(fmt='json')
    def testHtml(self):
        'IO: HTML dump (file+string)'
        self.tryDumpLoad(fmt='html',load=False)
        self.tryDumpLoadStr(fmt='html',load=False)
    def testPickle(self):
        'IO: pickle dump/load & format detection (file+string)'
        self.tryDumpLoad(fmt='pickle')
        self.tryDumpLoadStr(fmt='pickle',load=True)
    def testPickleEigen(self):
        'IO: pickling/unpickling Eigen objects'
        import pickle
        for o in [
            woo.Vector2(1,2),
            woo.Vector3(1,2,3),
            # woo.Vector4(1,2,3,4),
            woo.Vector6(1,2,3,4,5,6),
            woo.VectorX([1,2,3,4,5,6,7,8]),
            woo.VectorX([]),
            woo.Matrix3([[1,2,3],[4,5,6],[7,8,9]]),
            woo.MatrixX([[1,2,3,4],[5,6,7,8],[9,10,11,12],[13,14,15,16],[17,18,19,20]])
        ]:
            self.assertEqual(o,pickle.loads(pickle.dumps(o)))
    @unittest.skipIf('noxml' in woo.config.features,"Built without the 'xml' feature")
    def testXml(self):
        'IO: XML save/load & format detection'
        self.tryDumpLoad(ext='.xml')
    @unittest.skipIf('noxml' in woo.config.features,"Built without the 'xml' feature")
    def testXmlBz2(self):
        'IO: XML save/load (bzip2 compressed) & format detection'
        self.tryDumpLoad(ext='.xml.bz2')
    def testBin(self):
        'IO: binary save/load & format detection'
        self.tryDumpLoad(ext='.bin')
    def testBinGz(self):
        'IO: binary save/load (gzip compressed) & format detection'
        self.tryDumpLoad(ext='.bin.gz')
    def testInvalidFormat(self):
        'IO: invalid formats rejected'
        self.assertRaises(IOError,lambda: woo.master.scene.dem.par[0].dumps(format='bogus'))
    def testInvalidExtension(self):
        "IO: unrecognized extension rejected with format='auto'"
        self.assertRaises(IOError,lambda: woo.master.scene.dem.par[0].dump('/tmp/foo.barbaz',format='auto'))
    def testTmpStore(self):
        'IO: temporary store (loadTmp, saveTmp)'
        S=woo.master.scene
        for o in S.engines+[S.dem.par[0]]:
            o.saveTmp(quiet=True);
            o.__class__.loadTmp() # discard the result, but checks type
    def testDeepcopy(self):
        'IO: temporary store (Object.deepcopy)'
        S=woo.master.scene
        for o in S.engines+[S.dem.par[0]]:
            o2=o.deepcopy()
            self.assertTrue(type(o)==type(o2))
            self.assertTrue(id(o)!=id(o2))
    def testExprSpecialComments(self):
        'IO: special comments #: inside expr dumps'
        expr='''
#: import os
#: g=[]
#: for i in range(3):
#:   g.append((i+1)*os.getpid())
woo.dem.DemField(
    gravity=g
)
'''
        field=woo.dem.DemField.loads(expr,format='expr')
        import os
        self.assertEqual(field.gravity[0],os.getpid())
        self.assertEqual(field.gravity[1],2*os.getpid())
        self.assertEqual(field.gravity[2],3*os.getpid())
    def testExprHashColonOverride(self):
        'IO: special comments #% inside expr dumps'
        expr='''
#% a=3
#: b=a
woo.dem.DemField(gravity=(a,a,b))
'''
        field=woo.core.Object.loads(expr,format='expr',overrideHashPercent={'a':4})
        self.assertEqual(field.gravity[0],4) # not 3 as defined in the #: line
        self.assertRaises(NameError,lambda: woo.core.Object.loads(expr,format='expr',overrideHashPercent={'nonexistent':4}))


class TestSpecialDumpMethods(unittest.TestCase):
    def setUp(self):
        woo.master.reset()
        self.out=woo.master.tmpFilename()
    def testSceneLastDump_direct(self):
        'IO: Scene.lastSave set (Object._boostSave overridden)'
        woo.master.scene.save(self.out)
        self.assertTrue(woo.master.scene.lastSave==self.out)

class TestNoDumpAttributes(unittest.TestCase):
    def setUp(self):
        self.t=woo.core.WooTestClass()
        self.noDumpMaybe_T=[t for t in self.t._getAllTraits() if t.name=='noDumpMaybe'][0]
        self.noDumpAttr_T=[t for t in self.t._getAllTraits() if t.name=='noDumpAttr'][0]
        self.noDumpAttr2_T=[t for t in self.t._getAllTraits() if t.name=='noDumpAttr2'][0]
    def _test_cond(self,fmt,noDump):
        self.t.noDumpCondition=noDump
        self.t.noDumpMaybe=self.noDumpMaybe_T.ini+1
        t2=woo.core.Object.loads(self.t.dumps(format=fmt))
        self.assertEqual(t2.noDumpMaybe,self.noDumpMaybe_T.ini+(0 if noDump else 1))
    def testCond(self):
        'IO: conditional dumping to expr and JSON'
        for fmt in ['expr','json']:
            for noDump in (True,False):
                self._test_cond(fmt=fmt,noDump=noDump)
    def testUncond(self):
        'IO: unconditional noDump attribute (template param)'
        for fmt in ('expr','json'):
            self.t.noDumpAttr=self.noDumpAttr_T.ini+1
            self.t.noDumpAttr2=self.noDumpAttr2_T.ini+1
            t2=woo.core.Object.loads(self.t.dumps(format=fmt))
            self.assertEqual(t2.noDumpAttr,self.noDumpAttr_T.ini)
            self.assertEqual(t2.noDumpAttr2,self.noDumpAttr2_T.ini)


class TestArraySerialization(unittest.TestCase):
    @unittest.skipIf('pybind11' not in woo.config.features,'Temporarily disabled due to crashes Eigen/boost::python.')
    def testMatrixX(self):
        'IO: serialization of arrays'
        import sys
        t0=woo.core.WooTestClass()
        t0.matX=MatrixX([[0,1,2],[3,4,5]])
        out=woo.master.tmpFilename()
        t0.save(out)
        t1=woo.core.Object.load(out)
        self.assertTrue(t1.matX.rows()==2)
        self.assertTrue(t1.matX.cols()==3)
        self.assertTrue(t1.matX.sum()==15)
    def testBoostMultiArray(self):
        'IO: serialization of boost::multi_array'
        t0=woo.core.WooTestClass()
        t0.arr3d_set((2,2,2),[0,1,2,3,4,5,6,7])
        out=woo.master.tmpFilename()
        t0.save(out)
        t1=woo.core.Object.load(out)
        self.assertTrue(t1.arr3d==[[[0.0, 1.0], [2.0, 3.0]], [[4.0, 5.0], [6.0, 7.0]]])

        
