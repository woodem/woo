from minieigen import *
import woo.utils, woo.dem, woo.core

S=woo.master.scene=woo.core.Scene(fields=[woo.dem.DemField()])
if 0:
    S.periodic=True
    S.cell.setBox((1,1,1))

    woo.dem.BoxInlet(box=((0,0,0),S.cell.size0),massRate=0,collideExisting=False,materials=[woo.utils.defaultMaterial()],generator=woo.dem.PsdClumpGenerator(psdPts=[(.05,0),(.1,1)],clumps=[woo.dem.SphereClumpGeom(centers=[(0,0,-.06),(0,0,0),(0,0,.06)],radii=(.05,.08,.05),scaleProb=[(0,1.)])]))(S)
    sp=woo.dem.ShapePack()
    sp.fromDem(S,S.dem)
    sp.saveTxt('pack.txt')
else:
    sp=woo.dem.ShapePack()
    sp.loadTxt('pack.txt')
    sp.toDem(S,S.dem,mat=woo.utils.defaultMaterial())

S.engines=[woo.dem.WeirdTriaxControl(goal=(-.5,-.5,-.5),stressMask=0,mass=1,maxStrainRate=(.1,.1,.1),doneHook='S.stop()')]+woo.dem.DemField.minimalEngines()+[
    #woo.dem.PeriIsoCompressor(stresses=[-5e6],doneHook='S.stop()')
    # woo.dem.WeirdTriaxControl(goal=(-.5,-.5,-.5),stressMask=9,maxStrainRate=(-.1,-.1,-.1)),
]
#S.cell.nextGradV=Vector3(-.01,-.01,-.01).asDiagonal()
S.saveTmp()
S.run()
import woo.qt
woo.qt.View()
