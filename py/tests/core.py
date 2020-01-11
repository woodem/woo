# encoding: utf-8
# 2009 © Václav Šmilauer <eudoxos@arcig.cz>

"""
Core functionality (Scene in c++), such as accessing bodies, materials, interactions. Specific functionality tests should go to engines.py or elsewhere, not here.
"""
from builtins import range
from future.builtins import super
import unittest
import random
from minieigen import *
from math import *
import woo
from woo._customConverters import *
from woo import utils
from woo.core import *
from woo.dem import *
from woo.pre import *
try: from woo.sparc import *
except: pass
try: from woo.gl import *
except: pass
try: from woo.voro import *
except: pass
try: from woo.cld import *
except: pass
try: from woo.qt import *
except: pass

import woo
woo.master.usesApi=10104

## TODO tests
class TestInteractions(unittest.TestCase): pass
class TestForce(unittest.TestCase): pass
class TestTags(unittest.TestCase): pass 



class TestScene(unittest.TestCase):
    def setUp(self):
        self.scene=woo.core.Scene()
    #def _testTags(self):
    #    'Core: Scene.tags are str (not unicode) objects'
    #    S=self.scene
    #    S.tags['str']='asdfasd'
    #    S.tags['uni']=u'→ Σ'
    #    self.assertTrue(type(S.tags['str']==unicode))
    #    def tagError(S): S.tags['error']=234
    #    self.assertTrue(type(S.tags['uni']==unicode))
    #    self.assertRaises(TypeError,lambda: tagError(S))

class TestObjectInstantiation(unittest.TestCase):
    def setUp(self):
        self.t=woo.core.WooTestClass()
        # usually disabled and will be removed completely
        if hasattr(woo.core,'WooTestClassStatic'): self.ts=woo.core.WooTestClassStatic()
    def testClassCtors(self):
        "Core: correct types are instantiated"
        # correct instances created with Foo() syntax
        import woo.system
        for r in woo.system.childClasses(woo.core.Object):
            obj=r();
            self.assertTrue(obj.__class__==r,'Failed for '+r.__name__)
    def testRootDerivedCtors_attrs_few(self):
        "Core: class ctor's attributes"
        # attributes passed when using the Foo(attr1=value1,attr2=value2) syntax
        gm=Shape(color=1.); self.assertTrue(gm.color==1.)

    def testDeepcopy(self):
        'Core: Object.deepcopy'
        t=woo.core.WooTestClass()
        t.mass=0.
        t2=t.deepcopy(mass=1.0)    
        self.assertTrue(t2.mass==1.0 and t.mass==0.)
    def testDispatcherCtor(self):
        "Core: dispatcher ctors with functors"
        # dispatchers take list of their functors in the ctor
        # same functors are collapsed in one
        cld1=LawDispatcher([Law2_L6Geom_FrictPhys_IdealElPl(),Law2_L6Geom_FrictPhys_IdealElPl()]); self.assertTrue(len(cld1.functors)==1)
        # two different make two different
        cld2=LawDispatcher([Law2_L6Geom_FrictPhys_IdealElPl(),Law2_L6Geom_FrictPhys_LinEl6()]); self.assertTrue(len(cld2.functors)==2)
    def testContactLoopCtor(self):
        "Core: ContactLoop special ctor"
        # ContactLoop takes 3 lists
        id=ContactLoop([Cg2_Facet_Sphere_L6Geom(),Cg2_Sphere_Sphere_L6Geom()],[Cp2_FrictMat_FrictPhys()],[Law2_L6Geom_FrictPhys_IdealElPl()],)
        self.assertTrue(len(id.geoDisp.functors)==2)
        self.assertTrue(id.geoDisp.__class__==CGeomDispatcher)
        self.assertTrue(id.phyDisp.functors[0].__class__==Cp2_FrictMat_FrictPhys)
        self.assertTrue(id.lawDisp.functors[0].__class__==Law2_L6Geom_FrictPhys_IdealElPl)
    def testParallelEngineCtor(self):
        "Core: ParallelEngine special ctor"
        pe=ParallelEngine([InsertionSortCollider(),[BoundDispatcher(),ForceResetter()]])
        self.assertTrue(pe.slaves[0].__class__==InsertionSortCollider)
        self.assertTrue(len(pe.slaves[1])==2)
        pe.slaves=[]
        self.assertTrue(len(pe.slaves)==0)
    ##        
    ## testing incorrect operations that should raise exceptions
    ##
    def testWrongFunctorType(self):
        "Core: dispatcher and functor type mismatch is detected"
        # dispatchers accept only correct functors
        # pybind11: RuntimeError
        self.assertRaises((TypeError,RuntimeError),lambda: LawDispatcher([Bo1_Sphere_Aabb()]))
    def testInvalidAttr(self):
        'Core: invalid attribute access raises AttributeError'
        # accessing invalid attributes raises AttributeError
        self.assertRaises(AttributeError,lambda: Sphere(attributeThatDoesntExist=42))
        self.assertRaises(AttributeError,lambda: Sphere().attributeThatDoesntExist)
    ###
    ### shared ownership semantics
    ###
    def testShared(self):
        "Core: shared_ptr really shared"
        m=woo.utils.defaultMaterial()
        s1,s2=woo.utils.sphere((0,0,0),1,mat=m),woo.utils.sphere((0,0,0),2,mat=m)
        s1.mat.young=2342333
        self.assertTrue(s1.mat.young==s2.mat.young)
    def testSharedAfterReload(self):
        "Core: shared_ptr preserved when saving/loading"
        S=Scene(fields=[DemField()])
        m=woo.utils.defaultMaterial()
        S.dem.par.add([woo.utils.sphere((0,0,0),1,mat=m),woo.utils.sphere((0,0,0),2,mat=m)])
        S.saveTmp(quiet=True); S=Scene.loadTmp()
        S.dem.par[0].mat.young=9087438484
        self.assertTrue(S.dem.par[0].mat.young==S.dem.par[1].mat.young)
    ##
    ## attribute flags
    ##
    def testNoSave(self):
        'Core: Attr::noSave'
        # update bound of the particle
        t=self.t
        i0=t.noSaveAttr
        t.noSaveAttr=i0+10
        t.saveTmp('t')
        t2=woo.core.WooTestClass.loadTmp('t')
        # loaded copy has the default value
        self.assertTrue(t2.noSaveAttr==i0)
    @unittest.skipIf(not hasattr(woo.core,'WooTestClassStatic'),'Built without static attrs')
    def testNoSave_static(self):
        'Core: Attr::noSave (static)'
        ts=self.ts
        ts.noSave=344
        ts.namedEnum='one'
        ts.saveTmp('ts')
        ts.noSave=123
        self.assertTrue(ts.noSave==123)
        ts2=woo.core.WooTestClassStatic.loadTmp('ts')
        self.assertTrue(ts2.noSave==123)        # was not saved
        self.assertTrue(ts2.namedEnum=='one') # was saved
        # since it is static, self.ts is changed as well now
        self.assertTrue(ts.namedEnum==ts2.namedEnum)
        self.assertTrue(id(ts.namedEnum)==id(ts2.namedEnum))

    def testReadonly(self):
        'Core: Attr::readonly'
        self.assertTrue(self.t.meaning42==42)
        self.assertRaises(AttributeError,lambda: setattr(self.t,'meaning42',43))
    @unittest.skipIf(not hasattr(woo.core,'WooTestClassStatic'),'Built without static attrs')
    def testReaonly_static(self):
        'Core: Attr::readonly (static)'
        self.assertRaises(AttributeError,lambda: setattr(self.ts,'readonly',2))
    
    def testTriggerPostLoad(self):
        'Core: postLoad & Attr::triggerPostLoad'
        WTC=woo.core.WooTestClass
        # stage right after construction
        self.assertEqual(self.t.postLoadStage,WTC.postLoad_ctor)
        baz0=self.t.baz
        self.t.foo_incBaz=1 # assign whatever, baz should be incremented
        self.assertEqual(self.t.baz,baz0+1)
        self.assertEqual(self.t.postLoadStage,WTC.postLoad_foo)
        self.t.foo_incBaz=1 # again
        self.assertEqual(self.t.baz,baz0+2)
        self.t.bar_zeroBaz=1 # assign to reset baz
        self.assertEqual(self.t.baz,0)
        self.assertEqual(self.t.postLoadStage,WTC.postLoad_bar)
        # assign to baz to test postLoad again
        self.t.baz=-1
        self.assertTrue(self.t.postLoadStage==WTC.postLoad_baz)

    def testUnicodeStrConverter(self):
        'Core: std::string attributes can be assigned unicode string'
        t=woo.core.WooTestClass(strVar=u'abc')
        t.strVar=u'abc'
        self.assertTrue(t.strVar=='abc')
    def testNamedEnum(self):
        'Core: Attr::namedEnum'
        t=woo.core.WooTestClass()
        self.assertRaises(ValueError,lambda: setattr(t,'namedEnum','nonsense'))
        self.assertRaises(ValueError,lambda: setattr(t,'namedEnum',-2))
        self.assertRaises(TypeError,lambda: setattr(t,'namedEnum',[]))
        t.namedEnum='zero'
        self.assertTrue(t.namedEnum=='zero')
        # try with unicode string
        t.namedEnum=u'zero'
        self.assertTrue(t.namedEnum=='zero')
        t.namedEnum='nothing'
        self.assertTrue(t.namedEnum=='zero')
        t.namedEnum=0
        self.assertTrue(t.namedEnum=='zero')
        # ctor
        self.assertRaises(ValueError,lambda: woo.core.WooTestClass(namedEnum='nonsense'))
        tt=woo.core.WooTestClass(namedEnum='single')
        self.assertTrue(tt.namedEnum=='one')
    @unittest.skipIf(not hasattr(woo.core,'WooTestClassStatic'),'Built without static attrs')
    def testNamedEnum_static(self):
        'Core: Attr::namedEnum (static)'
        ts=self.ts
        self.assertRaises(ValueError,lambda: setattr(ts,'namedEnum','nonsense'))
        self.assertRaises(ValueError,lambda: setattr(ts,'namedEnum',-2))
        self.assertRaises(TypeError,lambda: setattr(ts,'namedEnum',[]))
        ts.namedEnum='zero'
        self.assertTrue(ts.namedEnum=='zero')
        ts.namedEnum=-1
        self.assertTrue(ts.namedEnum=='minus one')
        # test passing it in the ctor
        tt=woo.core.WooTestClass(namedEnum='NULL') # use the alternative name
        self.assertTrue(tt.namedEnum=='zero')
        tt=woo.core.WooTestClass(namedEnum=1) # use number
        self.assertTrue(tt.namedEnum=='one')

    def testBits(self):
        'Core: AttrTrait.bits accessors' 
        t=self.t
        # flags and bits read-write
        t.bits=1
        self.assertTrue(t.bit0)
        t.bit2=True
        self.assertTrue(t.bits==5)
        # flags read-only, bits midifiable
        self.assertRaises(AttributeError,lambda: setattr(t,'bitsRw',1))
        t.bit2rw=True
        t.bit3rw=True
        self.assertTrue(t.bitsRw==12)
        # both flags and bits read-only
        self.assertRaises(AttributeError,lambda: setattr(t,'bitsRo',1))
        self.assertRaises(AttributeError,lambda: setattr(t,'bit1ro',True))
        self.assertTrue(t.bitsRo==3)
        self.assertTrue((t.bit0ro,t.bit1ro,t.bit2ro)==(True,True,False))
    def testDeprecated(self):
        'Core: AttrTrait.deprecated raises exception on access'
        self.assertRaises(ValueError,lambda: getattr(self.t,'deprecatedAttr'))
        self.assertRaises(ValueError,lambda: setattr(self.t,'deprecatedAttr',1))
    ## not (yet) supported for static attributes
    # def testBits_static(self):

    def testHidden(self):
        'Core: Attr::hidden'
        # hidden attributes are not wrapped in python at all
        self.assertTrue(not hasattr(self.t,'hiddenAttr'))
    @unittest.skipIf(not hasattr(woo.core,'WooTestClassStatic'),'Built without static attrs')
    def testHidden_static(self):
        'Core: Attr::hidden (static)'
        self.assertRaises(AttributeError,lambda: getattr(self.ts,'hidden'))

    def testNotifyDead(self):
        'Core: PeriodicEngine::notifyDead'
        e=woo.core.WooTestPeriodicEngine()
        self.assertTrue(e.deadCounter==0)
        prev=e.deadCounter
        e.dead=True
        self.assertTrue(e.deadCounter>prev) # ideally, this should be 1, not 4 by now!!
        prev=e.deadCounter
        e.dead=True
        self.assertTrue(e.deadCounter>prev)
        prev=e.deadCounter
        e.dead=False
        self.assertTrue(e.deadCounter>prev)
    def testNodeDataCtorAssign(self):
        'Core: assign node data using shorthands in the ctor'
        n=woo.core.Node(pos=(1,2,3),dem=woo.dem.DemData(mass=100))
        self.assertTrue(n.dem.mass==100)
        # pybind11: RuntimeError
        self.assertRaises((TypeError,RuntimeError),lambda: woo.core.Node(dem=1))
        if 'gl' in woo.config.features:
            # type mismatch
            self.assertRaises(RuntimeError,lambda: woo.core.Node(dem=woo.gl.GlData()))

class TestLoop(unittest.TestCase):
    def setUp(self):
        woo.master.reset()
        woo.master.scene.fields=[DemField()]
        woo.master.scene.dt=1e-8
    def testSubstepping(self):
        'Loop: substepping'
        S=woo.master.scene
        S.engines=[PyRunner(1,'pass'),PyRunner(1,'pass'),PyRunner(1,'pass')]
        # value outside the loop
        self.assertTrue(S.subStep==-1)
        # O.subStep is meaningful when substepping
        S.subStepping=True
        S.one(); self.assertTrue(S.subStep==0)
        S.one(); self.assertTrue(S.subStep==1)
        # when substepping is turned off in the middle of the loop, the next step finishes the loop
        S.subStepping=False
        S.one(); self.assertTrue(S.subStep==-1)
        # subStep==0 inside the loop without substepping
        S.engines=[PyRunner(1,'if scene.subStep!=0: raise RuntimeError("scene.subStep!=0 inside the loop with Scene.subStepping==False!")')]
        S.one()
    def testEnginesModificationInsideLoop(self):
        'Loop: Scene.engines can be modified inside the loop transparently.'
        S=woo.master.scene
        S.engines=[
            PyRunner(stepPeriod=1,command='from woo.core import *; from woo.dem import *; scene.engines=[ForceResetter(),ForceResetter(),Leapfrog(reset=False)]'), # change engines here
            ForceResetter() # useless engine
        ]
        S.subStepping=True
        # run prologue and the first engine, which modifies O.engines
        S.one(); S.one(); self.assertTrue(S.subStep==1)
        self.assertTrue(len(S.engines)==3) # gives modified engine sequence transparently
        self.assertTrue(len(S._nextEngines)==3)
        self.assertTrue(len(S._currEngines)==2)
        S.one(); S.one(); # run the 2nd ForceResetter, and epilogue
        self.assertTrue(S.subStep==-1)
        # start the next step, nextEngines should replace engines automatically
        S.one()
        self.assertTrue(S.subStep==0)
        self.assertTrue(len(S._nextEngines)==0)
        self.assertTrue(len(S.engines)==3)
        self.assertTrue(len(S._currEngines)==3)
    def testDead(self):
        'Loop: dead engines are not run'
        S=woo.master.scene
        S.engines=[PyRunner(1,'pass',dead=True)]
        S.one(); self.assertTrue(S.engines[0].nDone==0)
    def testPausedContext(self):
        'Loop: "with Scene.paused()" context manager'
        import time
        S=woo.master.scene
        S.engines=[]
        S.run()
        with S.paused():
            i=S.step
            time.sleep(.1)
            self.assertTrue(i==S.step) # check there was no advance during those .1 secs
            self.assertTrue(S.running) # running should return true nevertheless
        time.sleep(.1)
        self.assertTrue(i<S.step) # check we run during those .1 secs again
        S.stop()
    def testNoneEngine(self):
        'Loop: None engine raises exception.'
        S=woo.master.scene
        self.assertRaises(RuntimeError,lambda: setattr(S,'engines',[ContactLoop(),None,ContactLoop()]))
    def testStopAtStep(self):
        'Loop: S.stopAtStep and S.run(steps=...)'
        S=woo.core.Scene(dt=1.)
        S.stopAtStep=100 # absolute value
        S.run(wait=True)
        self.assertEqual(S.step,100)
        S.run(steps=100,wait=True) # relative value
        self.assertEqual(S.step,200)
    def testStopAtTime(self):
        'Loop: S.stopAtTime and S.run(time=...)'
        S=woo.core.Scene(dt=1e-3)
        S.stopAtTime=1.0001 # absolute value
        S.run(wait=True)
        self.assertAlmostEqual(S.time,1.001,delta=1e-3)
        S.run(time=.5,wait=True) # relative value
        self.assertAlmostEqual(S.time,1.501,delta=1e-3)
    def testStopAtHook(self):
        'Loop: S.stopAtHook'
        S=woo.core.Scene(dt=1e-3)
        # both of them should trivver stopAtHook            
        S.stopAtTime=10e-3
        S.stopAtStep=1000
        S.lab.a=1
        S.lab._setWritable('a')
        S.stopAtHook='S.lab.a+=1'
        S.run(wait=True) # stopAtTime applies first
        self.assertEqual(S.lab.a,2)
        S.run(wait=True) # stopAtStep applies now
        self.assertEqual(S.lab.a,3)
    def testWait(self):
        'Loop: Scene.wait() returns only after the current step finished'
        S=woo.core.Scene(dt=1e-3,engines=[PyRunner(1,'import time; S.stop(); time.sleep(.3); S.lab.aa=True')])
        S.run(wait=True)
        self.assertTrue(hasattr(S.lab,'aa'))
    def testWaitForScenes(self):
        'Loop: Master.waitForScenes correctly handles reassignment of the master scene'
        S=woo.master.scene=woo.core.Scene(dt=1e-3,engines=[PyRunner(1,'import time; S.stop(); time.sleep(.3); woo.master.scene=woo.core.Scene(dt=1e-4,engines=[woo.core.PyRunner(1,"S.stop()")])')])
        S.run()
        woo.master.waitForScenes()
        self.assertTrue(woo.master.scene.dt==1e-4) # check master scene is the second one




        
class TestIO(unittest.TestCase):
    def testSaveAllClasses(self):
        'I/O: All classes can be saved and loaded with boost::serialization'
        import woo.system
        failed=set()
        for c in woo.system.childClasses(woo.core.Object):
            t=woo.core.WooTestClass()
            try:
                t.any=c()
                t.saveTmp(quiet=True)
                woo.master.loadTmpAny()
            except (RuntimeError,ValueError):
                print(20*'*'+' error with class '+c.__name__)
                import traceback
                traceback.print_exc()
                failed.add(c.__name__)
        failed=list(failed); failed.sort()
        print(80*'#'+'\nFailed classes were: '+' '.join(failed))
        self.assertTrue(len(failed)==0,'Failed classes were: '+' '.join(failed)+'\n'+80*'#')


class TestContact(unittest.TestCase):
    def setUp(self):
        self.S=S=woo.core.Scene(fields=[DemField()],engines=DemField.minimalEngines())
        for i in range(0,10):
            S.dem.par.add(woo.dem.Sphere.make((0,0,i),1.1))
        S.one()
    def testForceSign(self):
        'Contact: forceSign'
        S=self.S
        c45=S.dem.con[4,5]
        inc=(c45.id1==4)
        self.assertTrue(c45.forceSign(4)==(1 if inc else -1))
        self.assertTrue(c45.forceSign(5)==(-1 if inc else 1))
        self.assertRaises(RuntimeError,lambda: c45.forceSign(6))
        self.assertTrue(c45.forceSign(S.dem.par[4])==(1 if inc else -1))
        self.assertTrue(c45.forceSign(S.dem.par[5])==(-1 if inc else 1))
        self.assertRaises(RuntimeError,lambda: c45.forceSign(S.dem.par[6]))


class TestParticles(unittest.TestCase):
    def setUp(self):
        woo.master.reset()
        woo.master.scene.fields=[DemField()]
        S=woo.master.scene
        self.count=100
        S.dem.par.add([utils.sphere([random.random(),random.random(),random.random()],random.random()) for i in range(0,self.count)])
        random.seed()
    def testIterate(self):
        "Particles: iteration"
        counted=0
        S=woo.master.scene
        for b in S.dem.par: counted+=1
        self.assertTrue(counted==self.count)
    def testLen(self):
        "Particles: len(S.dem.par)"
        S=woo.master.scene
        self.assertTrue(len(S.dem.par)==self.count)
    def testRemove(self):
        "Particles: acessing removed particles raises IndexError"
        S=woo.master.scene
        S.dem.par.remove(0)
        self.assertRaises(IndexError,lambda: S.dem.par[0])
    def testRemoveShrink(self):
        "Particles: removing particles shrinks storage size"
        S=woo.master.scene
        for i in range(self.count-1,-1,-1):
            S.dem.par.remove(i)
            self.assertTrue(len(S.dem.par)==i)
    def testNegativeIndex(self):
        "Particles: negative index counts backwards (like python sequences)."
        S=woo.master.scene
        self.assertTrue(S.dem.par[-1]==S.dem.par[self.count-1])
    def testRemovedIterate(self):
        "Particles: iterator silently skips erased particles"
        S=woo.master.scene
        removed,counted=0,0
        for i in range(0,10):
            id=random.randint(0,self.count-1)
            if S.dem.par.exists(id): S.dem.par.remove(id); removed+=1
        for b in S.dem.par: counted+=1
        self.assertTrue(counted==self.count-removed)


class TestArrayAccu(unittest.TestCase):
    def setUp(self):
        self.t=woo.core.WooTestClass()
        self.N=woo.master.numThreads
    def testResize(self):
        'OpenMP array accu: resizing'
        self.assertEqual(len(self.t.aaccuRaw),0) # initial zero size
        for sz in (4,8,16,32,33):
            self.t.aaccuRaw=[i for i in range(0,sz)]
            r=self.t.aaccuRaw
            for i in range(0,sz):
                self.assertTrue(r[i][0]==i)  # first thread is assigned the value
                for n in range(1,self.N): self.assertTrue(r[i][n]==0) # other threads are reset
    def testPreserveResize(self):
        'OpenMP array accu: preserve old data on resize'
        self.t.aaccuRaw=(0,1)
        self.t.aaccuWriteThreads(2,[2]) # write whatever to index 2: resizes, but should preserve 0,1
        self.assertEqual(self.t.aaccuRaw[0][0],0)
        self.assertEqual(self.t.aaccuRaw[1][0],1)
    def testThreadWrite(self):
        'OpenMP array accu: concurrent writes'
        self.t.aaccuWriteThreads(0,list(range(0,self.N)))
        for i in range(0,self.N):
            self.assertEqual(self.t.aaccuRaw[0][i],i) # each thread has written its own index

class _TestPyClass(woo.core.Object,woo.pyderived.PyWooObject):
    'Sample pure-python class integrated into woo (mainly used with preprocessors), for use with :obj:`TestPyDerived`.'
    PAT=woo.pyderived.PyAttrTrait
    _attrTraits=[
        PAT(float,'aF',1.,'float attr'),
        PAT([float,],'aFF',[0.,1.,2.],'list of floats attr'),
        PAT(Vector2,'aV2',(0.,1.),'vector2 attr'),
        PAT([Vector2,],'aVV2',[(0.,0.),(1.,1.)],'list of vector2 attr'),
        PAT(woo.core.Node,'aNode',woo.core.Node(pos=(1,1,1)),'node attr'),
        PAT(woo.core.Node,'aNodeNone',None,'node attr, uninitialized'),
        PAT([woo.core.Node,],'aNNode',[woo.core.Node(pos=(1,1,1)),woo.core.Node(pos=(2,2,2))],'List of nodes'),
        PAT(float,'aF_trigger',1.,triggerPostLoad=True,doc='Float triggering postLoad, copying its value to aF'),
        PAT(int,'postLoadCounter',0,doc='counter for postLoad (readonly). Incremented by 1 after construction, incremented by 10 when assigning to aF_trigger.'),
        PAT(int,'deprecAttr',-1,deprecated=True,doc='deprecated, here should be the explanation.'),
        PAT(str,'sChoice','aa',choice=['aa','bb','cc'],doc='String choice attribute')
    ]
    def postLoad(self,I):
        if I is None:
            self.postLoadCounter+=1
        elif I=='aF_trigger':
            # print 'postLoad / aF_trigger'
            self.postLoadCounter+=10
            self.aF=self.aF_trigger
        else: raise RuntimeError(self.__class__.__name__+'.postLoad called with unknown attribute id %s'%I)
    def __new__(klass,**kw):
        self=super().__new__(klass)
        self.wooPyInit(klass,woo.core.Object,**kw)
        return self
    def __init__(self,**kw):
        woo.core.Object.__init__(self)
        self.wooPyInit(_TestPyClass,woo.core.Object,**kw)

class _TestPyClass2(_TestPyClass):
    'Python class deriving from python base class (which in turn derives from c++).'
    _PAT=woo.pyderived.PyAttrTrait
    def postLoad(self,I):
        if I=='f2' or I is None: self.f2counter+=1
        else: super(_TestPyClass2,self).postLoad(I)
    _attrTraits=[
        _PAT(int,'f2',0,triggerPostLoad=True,doc='Float attr in derived class'),
        _PAT(int,'f2counter',0,doc='Count how many times was f2 manipulated (to test triggerPostLoad in class with python parent)'),
    ]
    def __new__(klass,**kw):
        self=super().__new__(klass)
        self.wooPyInit(klass,_TestPyClass,**kw)
        return self
    def __init__(self,**kw):
        _TestPyClass.__init__(self)
        self.wooPyInit(_TestPyClass2,_TestPyClass,**kw)


class TestPyDerived(unittest.TestCase):
    import woo.pyderived, woo.core
    def setUp(self):
        self.t=_TestPyClass()
        self.t2=_TestPyClass2()
    def testTrigger(self):
        'PyDerived: postLoad triggers'
        # print 'postLoadCounter after ctor:',self.t.postLoadCounter
        self.assertTrue(self.t.postLoadCounter==1)
        self.t.aF_trigger=514.
        self.assertTrue(self.t.aF_trigger==514.)
        self.assertTrue(self.t.aF==514.)
        self.assertTrue(self.t.postLoadCounter==11)
    def testPickle(self):
        'PyDerived: deepcopy'
        self.assertTrue(self.t.aF==1.)
        self.assertTrue(self.t.aNode.pos==Vector3(1,1,1))
        self.t.aF=2.
        self.t.aNode.pos=Vector3(0,0,0)
        self.assertTrue(self.t.aF==2.)
        self.assertTrue(self.t.aNode.pos==Vector3(0,0,0))
        # pickle needs the class to be found in the module itself
        #    PicklingError: Can't pickle <class 'woo.tests.core._TestPyClass'>: it's not found as woo.tests.core._TestPyClass
        # this is fixed now the class is defined at the module level
        t2=self.t.deepcopy()
        self.assertTrue(t2.aF==2.)
        self.assertTrue(t2.aNode.pos==Vector3(0,0,0))
        self.t.aF=0.
        t3=self.t.deepcopy(aF=1.0) # with kw arg
        self.assertTrue(t3.aF==1. and self.t.aF==0.)
    def testTypeCoerceFloats(self):
        'PyDerived: type coercion (primitive types)'
        # numbers and number sequences            
        self.assertRaises(TypeError,lambda:setattr(self.t,'aF','asd'))
        self.assertRaises(TypeError,lambda:setattr(self.t,'aF','123')) # disallow conversion from strings
        self.assertRaises(TypeError,lambda:setattr(self.t,'aFF',(1,2,'ab')))
        self.assertRaises(TypeError,lambda:setattr(self.t,'aFF','ab'))
        self.assertRaises(TypeError,lambda:setattr(self.t,'aFF',[(1,2,3),(4,5,6)]))
        try: self.t.aFF=[]
        except: self.fail("Empty list not accepter for list of floats")
        try: self.t.aFF=Vector3(1,2,3)
        except: self.fail("Vector3 not accepted for list of floats")
        try: self.t.aV2=(0,1.)
        except: self.fail("2-tuple not accepted for Vector2")
    def testTypeCoerceObject(self):
        'PyDerived: type coercion (woo.core.Object)'
        # c++ objects
        try: self.t.aNode=None
        except: self.fail("None not accepted as woo.core.Node")
        self.assertRaises(TypeError,lambda:setattr(self.t,'aNode',woo.core.Scene()))
        # list of c++ objects
        self.assertRaises(TypeError,lambda:setattr(self.t,'aNNode',(woo.core.Node(),woo.core.Scene())))
        try: self.t.aNNode=[None,woo.core.Node()]
        except: self.fail("[None,Node] not accepted for list of Nodes")
    def testTypeCoerceCtor(self):
        'PyDerived: type coercion (ctor)'
        self.assertRaises(TypeError,lambda:_TestPyClass(aF='abc'))
    def testTraits(self):
        'PyDerived: PyAttrTraits'
        self.assertTrue(self.t._attrTraits[0].ini==1.)
        self.assertTrue(self.t._attrTraits[0].pyType==float)
    def testIniDefault(self):
        'PyDerived: default initialization'
        self.assertTrue(self.t.aF==1.)
        self.assertTrue(self.t.aFF==[0.,1.,2.])
        self.assertTrue(self.t.aNodeNone==None)
    def testIniUser(self):
        'PyDerived: user initialization'
        t2=_TestPyClass(aF=2.)
        self.assertTrue(t2.aF==2.)
        self.assertRaises(AttributeError,lambda: _TestPyClass(nonsense=123))
    def testStrValidation(self):
        'PyDerived: string choice is validated'
        try: self.t.sChoice='bb'
        except: self.fail("Valid choice value not accepted as new attribute value")
        self.assertRaises(ValueError, lambda: setattr(self.t,'sChoice','abc'))
        self.assertRaises(ValueError, lambda: _TestPyClass(sChoice='abc'))
        try: _TestPyClass(sChoice='bb')
        except: self.fail("Valid choice value not accepted in ctor")
    def testBoostSaveError(self):
        'PyDerived: refuses to save via Object::save (data loss; dump must be used instead)'
        self.assertRaises(IOError,lambda: self.t.save('whatever'))
        self.assertRaises(IOError,lambda: self.t2.save('whatever'))
    def testBoostDumpError(self):
        'PyDerived: refuses to dump with boost::serialization format (data loss)'
        self.assertRaises(IOError,lambda: self.t.dump('whatever.xml'))
        self.assertRaises(IOError,lambda: self.t2.dump('whatever.xml'))
    def testDeprecated(self):
        'PyDerived: deprecated attribute raises ValueError on access'
        self.assertRaises(ValueError,lambda: getattr(self.t,'deprecAttr'))
        self.assertRaises(ValueError,lambda: setattr(self.t,'deprecAttr',1))

    def testPickle2(self):
        'PyDerived: deepcopy (python parent)'
        self.t2.f2=3
        self.t2.aF=0
        tt=self.t2.deepcopy()
        self.assertTrue(tt.aF==0)
        self.assertTrue(tt.f2==3)
    def testIniUser(self):
        'PyDerived: user initialization (python parent)'
        t2=_TestPyClass2(aF=0,f2=3)
        self.assertTrue(t2.aF==0)
        self.assertTrue(t2.f2==3)
    def testTrigger(self):
        'PyDerived: postLoad trigger (python parent)'
        c1=self.t2.postLoadCounter
        c2=self.t2.f2counter
        self.t2.f2=4.
        self.t2.aF_trigger=5.
        self.assertTrue(self.t2.f2counter>c2)
        self.assertTrue(self.t2.postLoadCounter>c1)


        
    

