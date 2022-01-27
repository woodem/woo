# encoding: utf-8
# 2013 © Václav Šmilauer <eu@doxos.eu>

import unittest
from minieigen import *
import woo.core
import woo.dem

class TestSceneLabels(unittest.TestCase):
    'Test :obj:`LabelMapper` and related functionality.'
    def setUp(self):
        self.S=woo.core.Scene(fields=[woo.dem.DemField()])
    def testAccess(self):
        'LabelMapper: access'
        self.S.labels['abc']=123
        self.assertTrue(self.S.lab.abc==123)
        self.assertTrue(self.S.labels._whereIs('abc')==woo.core.LabelMapper.inPy)
        self.S.lab.ghi=456
        self.assertTrue(self.S.labels['ghi']==456)
    def testDel(self):
        'LabelMapper: delete'
        self.S.labels['foo']=123
        self.assertTrue(self.S.lab.foo==123)
        del self.S.lab.foo
        self.assertTrue(self.S.labels._whereIs('foo')==woo.core.LabelMapper.nowhere)
    def testSeq(self):
        'LabelMapper: sequences'
        o1,o2,o3=woo.core.Object(),woo.core.Object(),woo.core.Object()
        # from list
        self.S.lab.objs=[None,o1,o2,o3]
        self.assertTrue(self.S.lab.objs[1]==o1)
        self.assertTrue(self.S.labels._whereIs('objs')==woo.core.LabelMapper.inWooSeq)
        # from tuple
        self.S.lab.objs2=(None,o1,o2,o3)
        self.assertTrue(self.S.lab.objs2[1]==o1)
        self.assertTrue(self.S.labels._whereIs('objs2')==woo.core.LabelMapper.inWooSeq)
        # from indexed label
        self.S.labels['objs3[2]']=o2
        self.assertTrue(self.S.lab.objs3[0]==None)
        self.assertTrue(self.S.lab.objs3[2]==o2)
        self.assertTrue(len(self.S.lab.objs3)==3)
    def testString(self):
        'LabelMapper: strings and bytes'
        self.S.lab.emptyStr=''
        self.S.lab.emptyBytes=b''
        self.S.lab.someStr='abcdef'
        self.S.lab.someBytes=b'abcdef'
        self.assertTrue(self.S.lab._whereIs('emptyStr')==woo.core.LabelMapper.inPy)
        self.assertTrue(self.S.lab._whereIs('emptyBytes')==woo.core.LabelMapper.inPy)
        self.assertTrue(self.S.lab._whereIs('someStr')==woo.core.LabelMapper.inPy)
        self.assertTrue(self.S.lab._whereIs('someBytes')==woo.core.LabelMapper.inPy)
    def testMixedSeq(self):
        'LabelMapper: mixed sequences rejected, undetermined accepted'
        # mixed sequences
        o1,o2=woo.core.Object(),woo.core.Object()
        self.assertRaises(ValueError,lambda: setattr(self.S.lab,'ll',[o1,o2,12]))
        self.assertRaises(ValueError,lambda: setattr(self.S.lab,'ll',(12,23,o1)))
        # undetermined sequences are sequences of woo.core.Object
        try: self.S.lab.ll1=[]
        except: self.fail("[] rejected by LabelMapper.")
        try: self.S.lab.ll2=()
        except: self.fail("() rejected by LabelMapper.")
        try: self.S.lab.ll3=[None,None]
        except: self.fail("[None,None] rejected by LabelMapper.")
        # this is legitimate
        try: self.S.lab.mm=[o1,None]
        except: self.fail("[woo.Object,None] rejected by LabelMapper as mixed.")
        try: setattr(self.S.lab,'ll4[0]',[])
        except: self.fail("[] rejected in indexed label.")
        self.assertTrue(self.S.labels._whereIs('mm')==woo.core.LabelMapper.inWooSeq)
        # this as well
        try: self.S.lab.nn=[231,None]
        except: self.fail("[python-object,None] rejected by LabelMapper as mixed.")
        self.assertTrue(self.S.labels._whereIs('nn')==woo.core.LabelMapper.inPy)
    @unittest.skipIf('pybind11' in woo.config.features,'core.(ObjectList, NodeList, ...) not yet properly working with pybind11.')
    def testOpaqueSeqWithWooObjs(self):
        'LabelMapper: opaque sequence types with Woo objects are rejected'
        # this should be rejected
        ll=woo.core.ObjectList()
        ll.append(woo.core.Object())
        self.assertRaises(ValueError,lambda: setattr(self.S.lab,'aa',ll))
    def testShared(self):
        'LabelMapper: shared objects'
        o1=woo.core.Object()
        self.S.engines=[woo.core.PyRunner()]
        self.S.lab.someEngine=self.S.engines[0]
        self.assertTrue(self.S.labels._whereIs('someEngine')==woo.core.LabelMapper.inWoo)
        S2=self.S.deepcopy()
        self.assertTrue(S2.lab.someEngine==S2.engines[0])
    def testAutoLabel(self):
        'LabelMapper: labeled engines are added automatically'
        ee=woo.core.PyRunner(label='abc')
        self.S.engines=[ee]
        self.assertTrue(self.S.lab.abc==ee)
        self.assertTrue(self.S.labels._whereIs('abc')==woo.core.LabelMapper.inWoo)
    def testPseudoModules(self):
        'LabelMapper: pseudo-modules'
        S=self.S
        # using name which does not exist yet
        self.assertRaises(AttributeError,lambda: S.lab.abc)
        # using name which does not exist yet as pseudo-module
        self.assertRaises(NameError,lambda: setattr(S.lab,'abc.defg',1))
        S.lab._newModule('abc')
        self.assertTrue(S.lab._whereIs('abc')==woo.core.LabelMapper.inMod)
        S.lab.abc.a1=1
        # fail using method on proxyed pseudo-module
        self.assertRaises(AttributeError, lambda: S.lab.abc._newModule('a1')) 
        # deletion; should also delete abc.a1 automatically
        del S.lab['abc']
        self.assertRaises(NameError,lambda: getattr(S.lab,'abc'))
        self.assertRaises(NameError,lambda: getattr(S.lab,'abc.a1'))
        #self.assertRaises(ValueError, lambda: S.lab._newModule('abc.a1'))
        # fail when recreating existing module
        self.assertRaises(ValueError, lambda: S.lab._newModule('abc'))
        # nested
        S.lab._newModule('foo.bar')
        self.assertTrue(S.lab._whereIs('foo')==woo.core.LabelMapper.inMod)
        #self.assertTrue(S.lab._whereIs('foo.bar')==woo.core.LabelMapper.inMod)
        S.lab.foo.bar.bb=1
        self.assertTrue(S.lab.foo.bar.bb==1)
        # this should not raise any exception
        S.labels['foo.bar.baz[0]']=1
    def testWritable(self):
        self.S.lab.if_overwriting_this_causes_warning_it_is_a_bug=3
        self.S.lab._setWritable('if_overwriting_this_causes_warning_it_is_a_bug')
        # should not emit warning
        self.S.lab.if_overwriting_this_causes_warning_it_is_a_bug=4
    def testDir(self):
        'LabelMapper: __dir__'
        S=self.S
        S.lab._newModule('foo')
        S.lab.foo.bar=1
        self.assertTrue('bar' in S.lab.foo.__dir__())
    def testHasattr(self):
        'LabelMapper: __hasattr__'
        S=self.S
        S.lab._newModule('foo')
        S.lab.foo.bar=1
        S.lab.foo2=1
        self.assertTrue(hasattr(S.lab.foo,'bar'))
        self.assertTrue(hasattr(S.lab,'foo2'))
    def testGetattrIndexed(self):
        'LabelMapper: getattr with index'
        S=self.S
        S.engines=[woo.core.PyRunner(label='ee[1]'),woo.core.PyRunner(label='ee[2]')]
        self.assertTrue(getattr(S.lab,'ee[0]')==None)
        self.assertTrue(getattr(S.lab,'ee[1]')==S.engines[0])
        self.assertTrue(getattr(S.lab,'ee[2]')==S.engines[1])
    def testEngineLabels(self):
        'LabelMapper: engine/functor labels (mix of older tests)'
        S=self.S
        self.assertRaises(NameError,lambda: setattr(S,'engines',[woo.core.PyRunner(label='this is not a valid identifier name')]))
        #self.assertRaises(NameError,lambda: setattr(S,'engines',[PyRunner(label='foo'),PyRunner(label='foo[1]')]))
        cloop=woo.dem.ContactLoop([woo.dem.Cg2_Facet_Sphere_L6Geom(label='cg2fs'),woo.dem.Cg2_Sphere_Sphere_L6Geom(label='cg2ss')],[woo.dem.Cp2_FrictMat_FrictPhys(label='cp2ff')],[woo.dem.Law2_L6Geom_FrictPhys_IdealElPl(label='law2elpl')],)
        S.engines=[woo.core.PyRunner(label='foo'),woo.core.PyRunner(label='bar[2]'),woo.core.PyRunner(label='bar [0]'),cloop]
        # print S.lab.bar,type(S.lab.bar)
        self.assertTrue(hasattr(type(S.lab.bar),'__len__'))
        self.assertTrue(S.lab.foo==S.engines[0])
        self.assertTrue(S.lab.bar[0]==S.engines[2])
        self.assertTrue(S.lab.bar[1]==None)
        self.assertTrue(S.lab.bar[2]==S.engines[1])
        self.assertTrue(type(S.lab.cg2fs)==woo.dem.Cg2_Facet_Sphere_L6Geom)

