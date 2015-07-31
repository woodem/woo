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



