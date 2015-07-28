import woo.dem, woo.core, woo, woo.triangulated, math
S=woo.master.scene=woo.core.Scene(fields=[woo.dem.DemField(gravity=(0,0,10))])
S.dem.par.addClumped(*woo.triangulated.ribbedMill((0,0,0),(.5,0,0),radius=.5,majNum=15,majHt=.02,majTipAngle=1.5,minNum=5,minHt=.02,minTipAngle=1.5,wire=False,color=.5))
