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
