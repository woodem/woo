import woo, woo.batch, math
from woo.dem import *
from woo.core import *
from woo.gl import *
from minieigen import *
woo.master.usesApi=10101


S=woo.master.scene=Scene(fields=[DemField()],dtSafety=.9)

woo.batch.readParamsFromTable(S,noTableOk=True,
	young=24e9,
	ktDivKn=.2,

	sigmaT=3.5e6,
	tanPhi=0.8,
	epsCrackOnset=1e-4,
	relDuctility=100,

	distFactor=1.5,
	# dtSafety=.8,
	damping=0.4,
	#strainRateTension=.05,
	strainRateCompression=1e-2,
	#setSpeeds=True,
	# 1=tension, 2=compression (ANDed; 3=both)
	#doModes=3,

	specimenLength=.15,
	sphereRadius=1e-3,

	# isotropic confinement (should be negative)
	isoPrestress=0,
)

mat=ConcreteMat(young=S.lab.table.young,tanPhi=S.lab.table.tanPhi,ktDivKn=S.lab.table.ktDivKn,density=4800,sigmaT=S.lab.table.sigmaT,relDuctility=S.lab.table.relDuctility,epsCrackOnset=S.lab.table.epsCrackOnset,isoPrestress=S.lab.table.isoPrestress)

# sps=woo.pack.SpherePack()
S.lab.minRad=.17*S.lab.table.specimenLength
sp=woo.pack.randomDensePack(
	woo.pack.inHyperboloid((0,0,-.5*S.lab.table.specimenLength),(0,0,.5*S.lab.table.specimenLength),.25*S.lab.table.specimenLength,S.lab.minRad),
	spheresInCell=2000,
	radius=S.lab.table.sphereRadius,
	memoizeDb='/tmp/triaxPackCache.sqlite'
)
sp.cellSize=(0,0,0) # make aperiodic

sp.toSimulation(S,mat=mat)
S.engines=DemField.minimalEngines(model=woo.models.ContactModelSelector(name='concrete',mats=[mat],distFactor=S.lab.table.distFactor,damping=S.lab.table.damping),lawKw=dict(yieldSurfType='lin'))+[PyRunner(10,'addPlotData(S)')]

# take boundary nodes an prescribe velocity to those
lc=.5*S.lab.table.specimenLength-3*S.lab.table.sphereRadius
# impositions for top/bottom
S.lab.topImpose,S.lab.botImpose=VelocityAndReadForce(dir=Vector3.UnitZ),VelocityAndReadForce(dir=-Vector3.UnitZ)
for n in S.dem.nodes:
	if abs(n.pos[2])>lc:
		n.dem.impose=(S.lab.topImpose if n.pos[2]>0 else S.lab.botImpose)

def addPlotData(S):
	stage=(0 if S.lab.topImpose.vel>0 else 1)
	u=S.lab.topImpose.dist+S.lab.botImpose.dist
	eps=u/S.lab.table.specimenLength
	Ftop,Fbot=-S.lab.topImpose.sumF,-S.lab.botImpose.sumF
	sig=.5*(Ftop+Fbot)/(math.pi*S.lab.minRad**2)
	# in this case, we went to tension actually (dynamic), just don't record that as we then fail to detect pak in 
	if stage==0 and Ftop<0: pass
	else: S.plot.addData(eps=eps,sig=sig,u=u,Ftop=Ftop,Fbot=Fbot)
	halved=(Ftop<.5*(max(S.plot.data['Ftop'])) if stage==0 else Ftop>.5*min(S.plot.data['Ftop']))
	if S.step>1000 and halved:
		if stage==0:
			S.stop()
			p=woo.master.scene.plot
			p.splitData()
			S=woo.master.reload() # hope this will work and not freeze
			S.plot=p
			start(S,-1)
			S.run()
		else: S.stop()

# create initial contacts
S.one()
# reset distFactor
for f in S.lab.collider.boundDispatcher.functors+S.lab.contactLoop.geoDisp.functors:
	if hasattr(f,'distFactor'): f.distFactor=1.
# this is the initial state
S.gl.renderer.dispScale=(1000,1000,1000)
S.gl.demField.colorBy='ref. displacement'
S.gl.demField.vecAxis='xy'
S.plot.plots={'eps':('sig',),'u':('Ftop','Fbot')}

S.saveTmp('init')

def start(S,sign=+1):
	# impose velocity on bottom/top particles
	v=S.lab.table.strainRateCompression*S.lab.table.specimenLength
	# print sign*v
	S.lab.topImpose.vel=S.lab.botImpose.vel=sign*v
start(S,+1)
S.run()
