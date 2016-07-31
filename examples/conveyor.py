import woo.core, woo.dem, woo, woo.models, woo.pack
from minieigen import *
import math, numpy
woo.master.usesApi=10103

#
# the makeConveyor functions should go to some module
# but the interface must be generalized/improved before that is done
#

def makeConveyor(S,endPt,planeOri,wd0,wd1,trough,slope,fullLen,model,generator,feedSpeed,feedRate,halfThick=0.,edgeDiv=4,transLen=0,pulleyDiam=0,color=None,feedRelHt0=.2,debug=False):
    # XXX: the doc is not up-to-date
    '''Add conveyor object to the simulation. The cross-section is the following::

         ---__            __---         ↑  
             ^^-------^^    ∡ trough    ↓  ht2*
               ← wd0 →
         ←        wd1         →
         ← wd2*→                            (* are computed)

    :param endPt: endpoint of the conveyor; if discharge pulley is present, it will be the z-top of the pulley, otherwise it is the end of the conveyor as such.
    :type endPt: Vector3 in global space
    :param planeOri: orientation of the conveyor plane; local orientation is +z upwards, +x is the conveyor axis.
    :type planeOri: Quaternion
    :param wd0: widht of the middle (flat) part of the conveyor
    :param wd1: width of the the whole conveyor
    :param trough: troughing angle for the belt edges
    :type trough: rad
    :param slope: slope of the conveyor, relative to the plane
    :type slope: rad
    :param feedSpeed: axial speed of conveyor motion; will be applied as :obj:`woo.dem.Facet.fakeVel` to facets and :obj:`woo.dem.DemData.angVel` to discharge pulley, if present.
    :type speed: scalar
    :param mat: material instance for constructing facets/cylinder
    :param fullLen: length of the conveyor where full belt profile is used (i.e. excluding transition lenght (if any) and the pulley (if any)
    :param mask: `woo.dem.Particle.mask` for created mesh/cylinder
    :param edgeDiv: edges of the conveyor will be divided so that the transition is smoother (not effective withou transition zone)
    :param transLen: length of the transition zone, where the belt becomes linearly flat, keeping the width wd1.
    :param pulleyDiam: diameter of the discharge pulley (at the end of the belt); if non-positive, no pulley is created; the pulley should be globally axis-aligned (this limitation might be overcome in the future), locally is constructed along the local +y axis.
    :param halfThick: :obj:`woo.dem.Facet.halfThick` for the belt
    :param color: Color for facets/cylinder

    :rtype: (Vector3 begin of the full profile, Vector3 end of the full profile, Quaternion conveyor axis orientation) in global space
    '''
    
    wd2=.5*(wd1-wd0)
    ht2=wd2*math.tan(trough)
    mat0=model.mats[0]
    mat1=model.mats[1] if len(model.mats)>1 else mat0

    # create points in local coordinates:
    # A: beginning of the conveyor
    # B: axial point where the full profile ends, perhaps beginning of the transition zone
    # C: axial point of transition zone end, perhaps tangent point to the discharge pulley
    # D: top of the discharge pulley, if present, at (0,0,0)
    # E: center of the discharge pulley, if present

    slopeOri=Quaternion(Vector3.UnitY,-slope)

    if pulleyDiam:
        D=Vector3(0,0,0)
        E=D-Vector3(0,0,pulleyDiam/2.)
        C=E+slopeOri*Vector3(0,0,pulleyDiam/2.)
    else:
        C=D=Vector3(0,0,0)
    if transLen: B=C+slopeOri*Vector3(-transLen,0,0)
    else: B=C
    A=B+slopeOri*Vector3(-fullLen,0,0)

    # 3 profiles of the conveyor: at A, at B and at C
    # they are created in with +z perpendicularly up the conveyor, then rotated with slopeOri, then shifted to the point
    pyy=[-wd0/2.-wd2,-wd0/2.,wd0/2.,wd0/2.+wd2]
    # 4 points defining the belt full profile
    a,b,c,d=Vector3(0,pyy[0],ht2),Vector3(0,pyy[1],0),Vector3(0,pyy[2],0),Vector3(0,pyy[3],ht2)
    # finer profile for the transition zone (if needed)
    p0=[a+(i*1./edgeDiv)*(b-a) for i in range(0,edgeDiv+1)]+[c+(i*1./edgeDiv)*(d-c) for i in range(0,edgeDiv+1)]
    pA,pB1,pB2,pC=[slopeOri*p+A for p in (a,b,c,d)],[slopeOri*p+B for p in (a,b,c,d)],[slopeOri*p+B for p in p0],[slopeOri*Vector3(p[0],p[1],0)+C for p in p0]
    # print 'A: ',endPt+planeOri*A; print 'B: ',endPt+planeOri*B
    # convert to global coordinates here
    for lineSet in ([(pA,pB1),(pB2,pC)] if transLen else [(pA,pB1)]):
        surf=woo.pack.sweptPolylines2gtsSurface([[endPt+planeOri*p for p in pp] for pp in lineSet])
        S.dem.par.add(woo.pack.gtsSurface2Facets(surf,mat=mat0,wire=False,mask=woo.dem.DemField.defaultBoundaryMask,fakeVel=planeOri*slopeOri*Vector3(feedSpeed,0,0),color=color,halfThick=halfThick))
    # discharge pulley
    if pulleyDiam:
        axisVec=planeOri*Vector3.UnitY
        ax=int(numpy.argmax([axisVec.dot(Vector3.Unit(i)) for i in (0,1,2)]))
        if axisVec.dot(Vector3.Unit(ax))<0.99: raise ValueError("Discharge pulley must be aligned with a global axis (current pulley axis is %s, which is not close enough to %s)"%(axisVec,Vector3.Unit(ax)))
        pul=woo.dem.InfCylinder.make(endPt+planeOri*E,radius=pulleyDiam/2.,axis=ax,glAB=[(endPt+planeOri*Vector3(0,y,0))[ax] for y in (-wd1/2.,+wd1/2.)],mat=mat0,wire=False,mask=woo.dem.DemField.defaultBoundaryMask,color=color)
        pul.angVel=planeOri*Vector3(0,feedSpeed/(pulleyDiam/2.),0)
        S.dem.par.add(pul,nodes=True)

    convA,convB,convOri=(endPt+planeOri*A,endPt+planeOri*B,planeOri*slopeOri)

    cellLen=2*wd0  # feed periodicity
    feedRelHt0=.2  # height of feed, relative to width
    botLine=[Vector2(0,ht2),Vector2(wd2,0),Vector2(wd2+wd0,0),Vector2(wd1,ht2)] # floor for periodic cell (deposition)
    print('Preparing packing for conveyor feed, be patient ...')
    feedPack=woo.pack.makeBandFeedPack(dim=(cellLen,wd1,feedRelHt0*wd1),gen=generator,mat=mat1,gravity=S.dem.gravity,porosity=.5,damping=.3,memoizeDir='.',botLine=botLine,dontBlock=debug)
    if debug: return feedPack # scene object in this case
    
    inlet=woo.dem.ConveyorInlet(
        stepPeriod=200,
        material=mat1,
        shapePack=feedPack,
        barrierColor=.3,
        barrierLayer=-3., # 10x max radius
        color=0,
        glColor=.5,
        #color=.4, # random colors
        node=woo.core.Node(pos=convA,ori=convOri),
        mask=woo.dem.DemField.defaultInletMask,
        vel=feedSpeed,
        massRate=feedRate,
        label='feed',
        maxMass=-1, # not limited, until steady state is reached
        currRateSmooth=.2,
        # zTrim=...
    )
    S.engines+=[inlet]

#
# build simulation from here
#
S=woo.master.scene=woo.core.Scene(fields=[woo.dem.DemField(gravity=(0,0,-10))],engines=woo.dem.DemField.minimalEngines(model=model))
# contact model and materials to use
#model=woo.models.ContactModelSelector(name='pellet',mats=[woo.dem.PelletMat(young=1e7,density=2000)])
model=woo.models.ContactModelSelector(name='Schwarz',restitution=.8,mats=[woo.dem.FrictMat(young=1e7,density=2000)])
# particles to be put on the conveyor
generator=woo.dem.PsdCapsuleGenerator(psdPts=[Vector2(0.01,.0),Vector2(.02,.2),Vector2(.03,1.)],shaftRadiusRatio=(1.,2.))

makeConveyor(S,
    endPt=Vector3(0,0,0),planeOri=Quaternion.Identity,
    wd0=.2,wd1=.3,trough=math.radians(30),slope=math.radians(15),
    fullLen=.5,edgeDiv=5,transLen=.2,pulleyDiam=.2,color=.7,
    halfThick=0.0,
    feedSpeed=1.,feedRate=40*woo.unit['t/h'],
    model=model,
    generator=generator,
)
# define domain so that particles falling outside are removed
S.engines+=[woo.dem.BoxOutlet(box=((-1,-.5,-1),(.5,.5,.2)),inside=False,stepPeriod=200)]

