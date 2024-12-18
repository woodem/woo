'''
Test particle generator, that the resulting PSD curve matches the one on input.
'''
import unittest
from woo.core import *
from woo.dem import *
from minieigen import *
import numpy

class PsdSphereGeneratorTest(unittest.TestCase):
    def setUp(self):
        self.gen=PsdSphereGenerator(psdPts=[(.05,0),(.1,20),(.2,40),(.4,60),(.5,90),(.6,100)])
        self.mat=FrictMat(density=1000)
    def testMassDiscrete(self):
        'PSD: discrete mass-based generator'
        self.gen.mass=True; self.gen.discrete=True
        self.checkOk()
    def testMassContinuous(self):
        'PSD: continuous mass-based generator'
        self.gen.mass=True; self.gen.discrete=False
        self.checkOk(relDeltaInt=.05,relDeltaD=.1)
    def testNumDiscrete(self):
        'PSD: discrete number-based generator'
        self.gen.mass=False; self.gen.discrete=True
        self.checkOk()
    def testNumContinuous(self):
        'PSD: continuous number-based generator'
        self.gen.mass=False; self.gen.discrete=False
        self.checkOk()
    def testMonodisperse(self):
        'PSD: monodisperse packing'
        self.gen.psdPts=[(.05,0),(.05,1)]
        self.gen.mass=True; self.gen.discrete=False
        # this cannot be checked with numpy.trapz, do it manually
        for i in range(10000): self.gen(self.mat)
        (id,im),(od,om)=self.gen.inputPsd(normalize=False),self.gen.psd(normalize=False)
        self.assertTrue(id[0]==id[-1])
        self.assertAlmostEqual(im[-1],om[-1],delta=.04*im[-1])
    def testClippingSpuriousPoints(self):
        'PSD: clipping spurious values'
        self.gen.psdPts=[(0,0),(1,0),(5,5)]
        res=[(1,0),(5,1)]
        self.assertTrue(self.gen.psdPts==res)
        self.gen.psdPts=[(1,0),(5,2),(5,2),(5,2)]
        self.assertTrue(self.gen.psdPts==res)
    def testPsdTimeRange(self):
        'PSD: time range for computing PSD'
        # generate radii in different ranges at t=0 and t=1
        self.gen.psdPts=[(.1,0),(.2,1)]
        for i in range(50): self.gen(self.mat,0)
        self.gen.psdPts=[(1,0),(2,1)]
        for i in range(50): self.gen(self.mat,1)
        # now check that max and min in that time correspond
        psdA=self.gen.psd(normalize=True,num=10,tRange=(0,.5))
        psdB=self.gen.psd(normalize=True,num=10,tRange=(.5,2.))
        self.assertTrue(psdA[0][0]<.2 and psdA[0][-1]>.1)
        self.assertTrue(psdB[0][0]<2 and psdB[0][-1]>1.)
    def checkOk(self,relDeltaInt=.02,relDeltaD=.04):
        for i in range(10000): self.gen(self.mat)
        iPsd=self.gen.inputPsd(normalize=False)
        iPsdNcum=self.gen.inputPsd(normalize=False,cumulative=False,num=150)
         # scale by mass rather than number depending on the generator setup
        oPsd=self.gen.psd(mass=self.gen.mass,normalize=False,num=150)
        oPsdNcum=self.gen.psd(mass=self.gen.mass,normalize=False,num=150,cumulative=False)
        iInt=numpy.trapz(*iPsd)
        oInt=numpy.trapz(*oPsd)
        if 0: # enable to show graphical output
            import pylab
            pylab.figure()

            pylab.subplot(211)
            pylab.plot(*iPsd,label='in (%g)'%iInt)
            pylab.plot(*oPsd,label='out (%g)'%oInt)
            desc=('mass' if self.gen.mass else 'num','discrete' if self.gen.discrete else 'continuous')
            pylab.suptitle('%s-based %s generator (rel. area err %g)'%(desc[0],desc[1],(oInt-iInt)/iInt))
            # pylab.xlabel('Particle diameter')
            pylab.ylabel('Cumulative '+('mass' if self.gen.mass else 'number of particles'))
            pylab.grid(True)
            pylab.legend(loc='upper left')

            pylab.subplot(212)
            pylab.plot(*iPsdNcum,label='in')
            pylab.plot(*oPsdNcum,label='out')
            desc=('mass' if self.gen.mass else 'num','discrete' if self.gen.discrete else 'continuous')
            pylab.suptitle('%s-based %s generator (rel. area err %g)'%(desc[0],desc[1],(oInt-iInt)/iInt))
            pylab.xlabel('Particle diameter')
            pylab.ylabel('Histogram: '+('mass' if self.gen.mass else 'number of particles'))
            pylab.grid(True)
            pylab.legend(loc='upper left')

            pylab.savefig('/tmp/psd-test-%s-%s.png'%desc)
        # tolerance of 1%
        self.assertAlmostEqual(iInt,oInt,delta=relDeltaInt*iInt) 
        # check that integration minima and maxima match
        dMin,dMax=self.gen.psdPts[0][0],self.gen.psdPts[-1][0]
        # minimum diameter for discrete PSDs is the first one with fraction > 0
        if self.gen.discrete: dMin=[dd[0] for dd in self.gen.psdPts if dd[1]>0][0]
        # 3% tolerance here
        self.assertAlmostEqual(dMin,oPsd[0][0],delta=relDeltaD*dMin)
        self.assertAlmostEqual(dMax,oPsd[0][-1],delta=relDeltaD*dMax)
    def testNoDumpAttrs(self):
        'PSD: ParticleGenerator.genDiamMassTime not dumped into expr/json'
        for i in range(100): self.gen(self.mat)
        self.assertEqual(len(self.gen.genDiamMassTime),100)
        for fmt in ('expr','json'):
            g2=Object.loads(self.gen.dumps(format=fmt))
            self.assertEqual(len(g2.genDiamMassTime),0)



class BiasedPositionTest(unittest.TestCase):
    def testAxialBias(self):
        'Inlet: axial bias'
        bb=AxialBias(axis=0,d01=(2,1),fuzz=.1,phase=float('nan'))
        d0,d1=bb.d01
        for d in numpy.linspace(.5,2.5):
            p=bb.unitPos(d)[0]
            pMid=numpy.clip((d-d0)/(d1-d0),0,1)
            # print('d=%g,p=%g,pMid=%g,abs(p-pMid)=%g <= bb.fuzz/2.=%g',d,p,pMid,abs(p-pMid),bb.fuzz/2.)
            self.assertTrue(abs(p-pMid)<=bb.fuzz/2.)
