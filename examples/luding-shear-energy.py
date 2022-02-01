import woo, woo.plot, woo.utils
from woo import utils
from woo.core import *
from woo.dem import *
from woo import *
import woo.log
r=1e-2
delta=1e-7

m=LudingMat(
    young=30e9,k1DivKn=1,kaDivKn=0,ktDivKn=.2,krDivKn=0,kwDivKn=0,deltaLimRel=.01,
    tanPhi=.4,statR=0,statW=0,
    dynDivStat=1.,
    viscN=0,viscT=10,viscR=0,viscW=0
)

# XXX: inf kinetic energy with dynDivStat==1.
# XXX: deferred energy commit makes energy debugging difficult

S=woo.master.scene=Scene(
   fields=[DemField(
        distFactor=1.,
        parNodes=True, # add nodes of those particles, even if they are fixed
        par=[
            Sphere.make((0,0,0),r,fixed=True,wire=True,mat=m),
            Sphere.make((0,2*r-delta,0),r,fixed=True,wire=True,mat=m),
        ])
   ],
   engines=DemField.minimalEngines(model=woo.models.ContactModelSelector(name='luding'))+[
        PyRunner(1,'C=S.dem.con[0]; S.plot.addData(i=S.step,time=S.time,f=C.phys.force,t=C.phys.torque,uN=C.geom.uN,Ekrb=C.pA.Ekr,Ekra=C.pB.Ekr,vc=C.geom.vel,wc=C.geom.angVel,va=C.pA.vel,xa=C.pA.pos,wa=C.pA.angVel,vb=C.pB.vel,xb=C.pB.pos,wb=C.pB.angVel,f_y_max=abs(C.phys.force[0])*S.dem.par[0].mat.tanPhi,Wplast=C.phys.Wplast,Wvisc=C.phys.Wvisc,**S.energy)')
    ],
    plot=Plot(plots={
        'time':('wb_z'),
        #'time ':('va_x','va_y','va_z','vb_x','vb_y','vb_z'),
        #' time':('wc_x','wc_y','wc_z','vc_x','vc_y','vc_z'),
        #'  time':('xa_x','xa_y','xa_z','xb_x','xb_y','xb_z'),
        'time ':('f_y','f_y_max'),
        'time  ':('**S.energy',),
        'time   ':('Ekr','Wplast','Wvisc'),
    }),
    dtSafety=.01,
    trackEnergy=True
)
S.dem.nodes[1].dem.blocked='xyzXY'
S.dem.nodes[1].dem.angVel=(0,0,10)
# S.dem.nodes[1].dem.vel=(0,.1,0)

S.gl.renderer.rotScale=10
S.gl.renderer.scaleOn=True
S.throttle=1e-3

S.stopAtStep=2000
S.saveTmp()
S.run()
S.plot.plot()
#O.reload()

