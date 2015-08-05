import woo, woo.pack
from woo.dem import *
from woo.core import *

S=woo.master.scene=woo.core.Scene(fields=[DemField()])
# predicate to generate packing for
predicate=woo.pack.inHyperboloid((0,0,-.5*.15),(0,0,.5*.15),.25*.15,.17*.15)


# with capsules and randomDensePack2 (sp2 is a ShapePack)
generator=woo.dem.PsdCapsuleGenerator(psdPts=[(.005,0),(.008,.4),(.012,.7),(.015,1)],shaftRadiusRatio=(.8,1.5))
# porosity must be specified so that the initial cell size can be estimated
# it can be something like .4 for sphere, but less for non-spherical generators (capsules, clumps)
sp=woo.pack.randomDensePack2(predicate,generator,porosity=.1,memoizeDir='.')
sp.filtered(predicate).toDem(S,S.dem,mat=FrictMat())


# with spheres and randomDensePack (sp2 is a ShapePack)
generator=woo.dem.PsdSphereGenerator(psdPts=[(.005,0),(.008,.4),(.012,.7),(.015,1)])
sp2=woo.pack.randomDensePack2(predicate,generator,porosity=.3,memoizeDir='.')
sp2.translate((.1,0,0))
sp2.toDem(S,S.dem,mat=FrictMat())


# with the original randomDensePack (sp3 is a SpherePack)
sp3=woo.pack.randomDensePack(predicate,spheresInCell=2000,radius=.004,rRelFuzz=.5,memoizeDb='/tmp/triaxPackCache.sqlite')
sp3.translate((0,0.1,0))
sp3.toDem(S,S.dem,mat=FrictMat())

import woo.gl
S.gl.demField.colorBy='radius'
