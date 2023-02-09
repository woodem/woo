from minieigen import *
from math import *
import numpy as np
import unittest

class TestMinieigen(unittest.TestCase):
    def test_Vector(self):
        'Eigen: Vector'
        self.assertTrue(Vector3(0,0,0)==Vector3.Zero)
        self.assertFalse(Vector3(0,0,0)!=Vector3.Zero)
        self.assertEqual(Vector3(1,2,3).sum(),6)
    def test_AlignedBox(self):
        'Eigen: AlignedBox'
        self.assertEqual(len(AlignedBox3()),2)
        self.assertEqual(len(AlignedBox2()),2)
