import woo.core, woo.dem
from minieigen import *
woo.master.usesApi=10103

# material we will use for both boundary and particles
m0=woo.dem.FrictMat(density=3000,young=1e5)
# period for running engines which only run sometimes
per=100 
# inlet is producing particles
inlet=woo.dem.BoxInlet(
    box=((-.03,-.03,.1),(.03,.03,.15)), # volume of inlet
    massRate=.5, # mass rate at which particles are being generated
    materials=[m0],
    generator=woo.dem.PsdSphereGenerator(psdPts=[(.005,0),(.01,1)]),
    stepPeriod=per,
    glColor=.9,
)
# outlet defines domain where particles will be deleted
outlet=woo.dem.BoxOutlet(
    box=((-.06,-.06,-0.01),(.06,.06,.15)),
    inside=False,
    stepPeriod=per, # delete what is outside
    currRateSmooth=.5, # smoothing factor to make fluctuations smaller (smoothed value is consumed by DetectSteadyState)
    glColor=.5,
)
# detector of steady state
steady=woo.dem.DetectSteadyState(
    inlets=[inlet],  # one inlet here, but could be more (sum of fluxes is influx)
    outlets=[outlet], # one outlet here, but could be more (sum of fluxes is efflux)
    relFlow=1., # wait for 1.*efflux >= influx, before entering 'trans' stage
    hookTrans='print("hookTrans")', # do this when 'trans' is entered
    waitTrans=.5, # stay .5s in 'trans' stage, before entering steady stage
    hookSteady='print("hookSteady")', # do this when 'stead' is entered
    waitSteady=1, # stay 1s in 'steady' stage
    hookDone='S.stop(); engine.dead; print("hookDone, stopping")', # do this when 'done' is entered (steady finished)
    stepPeriod=per, # run only every *per* steps, just like inlets and outlets
    rateSmooth=.9, # if defined, apply extra smoothing to influx and efflux values (not really necessary)
    label='steady',
)
# scene object
S=woo.master.scene=woo.core.Scene(
    fields=[woo.dem.DemField(gravity=(0,0,-10),
        # cylinder with open top (cup-shaped) to hold some particles
        par=woo.triangulated.cylinder(Vector3(0,0,0),Vector3(0,0,.05),.05,mat=m0,capA=True,capB=False)
    )],
    # usual engine setup, adding inlet, outlet, steady detector and plotting
    engines=woo.dem.DemField.minimalEngines(damping=.3)+[
        inlet,outlet,steady,woo.core.PyRunner(per,'S.plot.autoData()')
    ]
)
# what to plot
S.plot.plots={'t=S.time':('influx=S.lab.steady.influx','efflux=S.lab.steady.efflux',None,'stageNum=S.lab.steady.stageNum')}
# color particles by velocity
S.gl.demField.colorBy='velocity'
S.gl.demField.colorRange.mnmx=(0,.5)
# save for reloading
S.saveTmp()
# let's go
import woo.qt
woo.qt.View()
#S.run()
#S.plot.plot()
