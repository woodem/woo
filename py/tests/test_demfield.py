'''
Test loading and saving woo objects in various formats
'''
import woo
import unittest
from woo.core import *
from woo.dem import *
from minieigen import *

class TestDemField(unittest.TestCase):
    def testGuessMoving(self):
        'DEM: correctly guess that Node.dem needs motion integration'
        d=DemData(blocked='',mass=0)
        self.assertTrue(d.guessMoving()==False) # zero mass, don't move
        d=DemData(blocked='xyz',mass=0)
        self.assertTrue(d.guessMoving()==False) # zero mass, don't move
        d=DemData(blocked='xyzXY',mass=1)
        self.assertTrue(d.guessMoving()==True)  # something not blocked, move
        d=DemData(blocked='xyzXYZ',mass=1)
        self.assertTrue(d.guessMoving()==False) # everything blocked, not move
        d=DemData(blocked='xyzXYZ',vel=(1,1,1),mass=1)
        self.assertTrue(d.guessMoving()==True)  # velocity assigned, move

class TestContactLoop(unittest.TestCase):
    def testUpdatePhys(self):
        'DEM: ContactLoop.updatePhys: never, always, once'
        for up in 'never','always','once':
            E0,E1=1e3,1e6
            m=FrictMat(young=E0)
            S=Scene(fields=[DemField(par=[Sphere.make((0,0,0),.6,mat=m),Sphere.make((0,0,1),.6,mat=m)])],engines=DemField.minimalEngines(),dtSafety=1e-8,dt=1e-8)
            # step one creates the contact
            S.one(); c=S.dem.con[0]
            kn0=c.phys.kn
            # change material, do step
            S.lab.contactLoop.updatePhys=up # assign here, was 'never' up to now (the default)
            m.young=E1
            S.one()
            kn1=c.phys.kn
            # those two updated
            if up in ('always','once'): self.assertAlmostEqual(kn0*(E1/E0),kn1)
            else: self.assertAlmostEqual(kn0,c.phys.kn)
            # change material back
            m.young=E0
            S.one()
            if up=='always':
                self.assertTrue(S.lab.contactLoop.updatePhys=='always')
                self.assertAlmostEqual(kn0,c.phys.kn)
            else:
                self.assertTrue(S.lab.contactLoop.updatePhys=='never') # once changed to never, or just never
                self.assertEqual(kn1,c.phys.kn)


class TestImpose(unittest.TestCase):
    def testCombinedImpose(self):
        'DEM: CombinedImpose created from Impose()+Impose()'
        a,b,c=CircularOrbit(),ReadForce(),ConstantForce()
        d=a+b
        # normal + normal, instantiates CombinedImpose
        self.assertTrue(d.imps==[a,b])
        self.assertTrue(d.what==(a.what | b.what)) # what is OR'd from imps
        # flattening additions
        # combined + combined
        self.assertTrue((d+d).imps==[a,b,a,b])
        # combined + normal
        self.assertTrue((d+c).imps==[a,b,c])
        # normal + combined
        self.assertTrue((c+d).imps==[c,a,b])
    def testCombinedImposeFunction(self):
        'DEM: CombinedImpose combines impositions'
        a=VariableAlignedRotation(axis=0,timeAngVel=[(0,0),(1,1),(2,1)],wrap=False)
        b=VariableVelocity3d(times=[0,1,2],vels=[(0,0,0),(3,3,3),(3,3,3)])
        c=InfCylinder.make((0,0,0),radius=.2,axis=1)
        c.impose=a+b # impose both
        S=woo.core.Scene(fields=[DemField(par=[c])],engines=DemField.minimalEngines(verletDist=0.),dt=1e-3,stopAtTime=1.2)
        S.run(); S.wait()
        self.assertTrue(c.vel==Vector3(3,3,3))
        self.assertTrue(c.angVel[0]==1.)
    def testUselessRotationImpose(self):
        'DEM: selfTest error when imposing rotation on aspherical particles (would have no effect)'
        for i in VariableAlignedRotation(axis=0,timeAngVel=[(0,0)]),InterpolatedMotion(),Local6Dofs(whats=(0,0,0,1,1,1)),VariableVelocity3d(times=[0,1],vels=[(0,0,0),(0,0,0)],angVels=[(0,0,0),(0,0,0)]):
            a=Capsule.make((0,0,0),radius=.3,shaft=.6,ori=Quaternion((1,0,0),1))
            a.impose=i
            S=woo.core.Scene(fields=[DemField(par=[a])],engines=DemField.minimalEngines(verletDist=0.))
            self.assertRaises(RuntimeError,lambda : S.selfTest())
    def testInterpolatedMotion(self):
        'DEM: InterpolatedMotion computes correct position and rotation'
        import math
        a=Capsule.make((0,0,0),radius=.3,shaft=.6,ori=Quaternion.Identity)
        p1,p2=Vector3(1,1,1),Vector3(0,0,0)
        q1,q2=Quaternion((.3,.4,.5),3.1415).normalized(),Quaternion((-.5,-.4,1),-.7).normalized()
        a.impose=InterpolatedMotion(poss=[a.pos,p1,p2],oris=[a.ori,q1,q2],times=[0,.2,.4])
        a.blocked='xyzXYZ' # must be blocked otherwise we get an error from selfTest
        S=woo.core.Scene(fields=[DemField(par=[a])],engines=DemField.minimalEngines(verletDist=0),dt=1e-1)
        S.run(10,True) # run in fg
        dAngle=(q2.conjugate()*a.ori).toAxisAngle()[1]
        if dAngle>math.pi: dAngle-=2*math.pi
        dPos=(a.pos-p2).norm()
        self.assertAlmostEqual(dAngle,0)
        self.assertAlmostEqual(dPos,0)
    def testVariableVelocity3d_rot(self):
        'DEM: VariableVelocity3d imposition (angular velocity)'
        a=Sphere.make((0,0,0),radius=.3)
        t1,omega1=.2,1
        a.impose=VariableVelocity3d(times=[0,t1],vels=[(0,0,0),(0,0,0)],angVels=[(0,0,0),(omega1,0,0)])
        S=woo.core.Scene(fields=[DemField(par=[a])],engines=DemField.minimalEngines(verletDist=0.),stopAtTime=t1,dt=1e-4)
        S.run(wait=True)
        self.assertAlmostEqual(S.dem.nodes[0].ori.toAxisAngle()[1],.5*omega1*t1,delta=1e-3)

