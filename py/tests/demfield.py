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
		self.assert_(d.guessMoving()==False) # zero mass, don't move
		d=DemData(blocked='xyz',mass=0)
		self.assert_(d.guessMoving()==False) # zero mass, don't move
		d=DemData(blocked='xyzXY',mass=1)
		self.assert_(d.guessMoving()==True)  # something not blocked, move
		d=DemData(blocked='xyzXYZ',mass=1)
		self.assert_(d.guessMoving()==False) # everything blocked, not move
		d=DemData(blocked='xyzXYZ',vel=(1,1,1),mass=1)
		self.assert_(d.guessMoving()==True)  # velocity assigned, move

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
				self.assert_(S.lab.contactLoop.updatePhys=='always')
				self.assertAlmostEqual(kn0,c.phys.kn)
			else:
				self.assert_(S.lab.contactLoop.updatePhys=='never') # once changed to never, or just never
				self.assertEqual(kn1,c.phys.kn)


class TestImpose(unittest.TestCase):
	def testCombinedImpose(self):
		'DEM: CombinedImpose created from Impose()+Impose()'
		a,b,c=CircularOrbit(),ReadForce(),ConstantForce()
		d=a+b
		# normal + normal, instantiates CombinedImpose
		self.assert_(d.imps==[a,b])
		self.assert_(d.what==(a.what | b.what)) # what is OR'd from imps
		# flattening additions
		# combined + combined
		self.assert_((d+d).imps==[a,b,a,b])
		# combined + normal
		self.assert_((d+c).imps==[a,b,c])
		# normal + combined
		self.assert_((c+d).imps==[c,a,b])
	def testCombinedImposeFunction(self):
		'DEM: CombinedImpose combines impositions'
		a=VariableAlignedRotation(axis=0,timeAngVel=[(0,0),(1,1),(2,1)],wrap=False)
		b=VariableVelocity3d(times=[0,1,2],vels=[(0,0,0),(3,3,3),(3,3,3)])
		c=InfCylinder.make((0,0,0),radius=.2,axis=1)
		c.impose=a+b # impose both
		S=woo.core.Scene(fields=[DemField(par=[c])],engines=DemField.minimalEngines(),dt=1e-3,stopAtTime=1.2)
		S.run(); S.wait()
		self.assert_(c.vel==Vector3(3,3,3))
		self.assert_(c.angVel[0]==1.)
	def testUselessRotationImpose(self):
		'DEM: selfTest errro when imposing rotation on aspherical particles (would have no effect)'
		for i in VariableAlignedRotation(axis=0,timeAngVel=[(0,0)]),InterpolatedMotion(),Local6Dofs(whats=(0,0,0,1,1,1)):
			a=Capsule.make((0,0,0),radius=.3,shaft=.6,ori=Quaternion((1,0,0),1))
			a.impose=i
			S=woo.core.Scene(fields=[DemField(par=[a])],engines=DemField.minimalEngines())
			self.assertRaises(RuntimeError,lambda : S.selfTest())
