import woo, woo.utils, woo.pack, woo.plot
from woo.dem import *
from woo.core import *
woo.master.usesApi=10103
woo.master.scene=S=Scene(fields=[DemField(gravity=(0,0,-10))])
mat=woo.utils.defaultMaterial()
sp=woo.pack.SpherePack()
sp.makeCloud((0,0,0),(10,10,10),.4,rRelFuzz=.5)
sp.toSimulation(S,mat=mat)
S.dem.par.add(Wall.make(0,axis=2,sense=1,mat=mat))
S.engines=DemField.minimalEngines(damping=0.4)
# count all new and deleted contacts
S.lab.cHook=S.lab.contactLoop.hook=CountContactsHook()
S.saveTmp()
S.run(4000)
S.wait()
print("Contact counts: created ",S.lab.cHook.nNew,'deleted',S.lab.cHook.nDel)
