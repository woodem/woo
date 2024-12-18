# encoding: utf-8

from woo.dem import *
from woo.fem import *
import woo.core
import woo.dem
import woo.pyderived
import woo.models
import math
from minieigen import *

class FallingHorse(woo.core.Preprocessor,woo.pyderived.PyWooObject):
    '''Preprocessor for the falling horse simulation. The falling horse is historically the demo of Woo. IT shows importing triangulated surfaces, filling imported geometry with particle arrangement, selection of material model, export for VTK (Paraview) and the :obj:`woo.dem.FlowAnalysis` tool. \n\n.. youtube:: LB3T6sBdwz0
    '''
    _classTraits=None
    _PAT=woo.pyderived.PyAttrTrait # less typing
    _attrTraits=[
        _PAT(float,'radius',.002,unit='mm',startGroup='General',doc='Radius of spheres (fill of the upper horse)'),
        _PAT(float,'relGap',.25,doc='Gap between particles in pattern, relative to :obj:`radius`'),
        _PAT(float,'halfThick',.002,unit='mm',doc='Half-thickness of the mesh.'),
        _PAT(float,'relEkStop',.02,'Stop when kinetic energy drops below this fraction of gravity work (and step number is greater than 100)'),
        _PAT(Vector3,'gravity',(0,0,-9.81),'Gravity acceleration vector'),
        _PAT(Vector3,'dir',(0,0,1),'Direction of the upper horse from the lower one. The default is upwards. Will be normalized automatically.'),
        _PAT(str,'pattern','hexa',choice=['hexa','ortho'],doc='Pattern to use when filling the volume with spheres'),
        _PAT(woo.models.ContactModelSelector,'model',woo.models.ContactModelSelector(name='Schwarz',restitution=.7,numMat=(1,2),matDesc=['particles','mesh'],mats=[woo.dem.HertzMat(density=1e4,young=1e6,surfEnergy=.4,alpha=.6)]),doc='Select contact model. The first material is for particles; the second, optional, material, is for the meshed horse (the first material is used if there is no second one).'),
        # _PAT(woo.dem.FrictMat,'meshMat',None,'Material for the meshed horse; if not given, :obj:`mat` is used here as well.'),
        _PAT(bool,'deformable',False,startGroup='Deformability',doc='Whether the meshed horse is deformable. Note that deformable horse does not track energy and disables plotting.'),
        _PAT(bool,'stand',False,doc='Whether the bottoms of the legs should be fixed (applicable with :obj:`deformable` only)'),
        _PAT(float,'meshDamping',.03,doc='Damping for mesh nodes; only used when then contact :obj:`model` sets zero :obj:`~woo.dem.Leapfrog.damping`. In that case, :obj:`Leapfrog.damping <woo.dem.Leapfrog.damping>` is set to :obj:`meshDamping` and all other nodes set :obj:`woo.dem.DemData.dampingSkip`.'),
        _PAT(float,'dtSafety',.3,startGroup='Tunables',doc='Safety factor for :obj:`woo.utils.pWaveDt` and :obj:`woo.dem.DynDt`.'),
        _PAT(str,'reportFmt',"/tmp/{tid}.xhtml",filename=True,startGroup="Outputs",doc="Report output format; :obj:`Scene.tags <woo.core.Scene.tags>` can be used."),
        _PAT(int,'vtkStep',40,"How often should :obj:`woo.dem.VtkExport` run. If non-positive, never run the export."),
        _PAT(int,'vtkFlowStep',40,"How often should :obj:`VtkFlowExport` run. If non-positive, never run."),
        _PAT(str,'vtkPrefix',"/tmp/{tid}-",filename=True,doc="Prefix for saving :obj:`woo.dem.VtkExport` and :obj:`woo.dem.VtkFlowExport` data; formatted with ``format()`` providing :obj:`woo.core.Scene.tags` as keys."),
        _PAT(bool,'grid',False,'Use grid collider (experimental)'),
    ]
    def __new__(klass,**kw):
        self=super().__new__(klass)
        self.wooPyInit(FallingHorse,woo.core.Preprocessor,**kw)
        return self
    def __init__(self,**kw):
        woo.core.Preprocessor.__init__(self)
        self.wooPyInit(FallingHorse,woo.core.Preprocessor,**kw)
    def __call__(self):
        # preprocessor builds the simulation when called
        return prepareHorse(self)

def prepareHorse(pre):
    import sys, os, woo
    woo.master.usesApi=10102
    import woo.gts as gts # not sure whether this is necessary
    import woo.pack, woo.utils, woo.core, woo
    S=woo.core.Scene(fields=[DemField()])
    for a in ['reportFmt','vtkPrefix']: setattr(pre,a,woo.utils.fixWindowsPath(getattr(pre,a)))
    S.pre=pre.deepcopy() # so that our manipulation does not affect input fields
    S.dem.gravity=pre.gravity
    # if not pre.meshMat: pre.meshMat=pre.mat.deepcopy()

    mat=pre.model.mats[0]
    meshMat=(pre.model.mats[1] if len(pre.model.mats)>1 else mat)
    

    # load the horse
    import importlib.resources
    with (importlib.resources.files('woo')/'data/horse.coarse.gts').open('rb') as data:
        surf=gts.read(data)
    
    if not surf.is_closed(): raise RuntimeError('Horse surface not closed?!')
    pred=woo.pack.inGtsSurface(surf)
    aabb=pred.aabb()
    # filled horse above
    if pre.pattern=='hexa': packer=woo.pack.regularHexa
    elif pre.pattern=='ortho': packer=woo.pack.regularOrtho
    else: raise ValueError('FallingHorse.pattern must be one of hexa, ortho (not %s)'%pre.pattern)
    S.dem.par.add(packer(pred,radius=pre.radius,gap=pre.relGap*pre.radius,mat=mat,retSpherePack=False))
    # meshed horse below
    xSpan,ySpan,zSpan=aabb.sizes() # aabb[1][0]-aabb[0][0],aabb[1][1]-aabb[0][1],aabb[1][2]-aabb[0][2]
    if pre.dir.squaredNorm()>0.:
        trans=-pre.dir.normalized()*zSpan
    else: trans=Vector3(0,0,-zSpan)
    surf.translate(*trans)
    zMin=aabb[0][2]+trans[2]-pre.halfThick
    xMin,yMin,xMax,yMax=aabb[0][0]-zSpan+trans[0],aabb[0][1]-zSpan+trans[1],aabb[1][0]+zSpan+trans[0],aabb[1][1]+zSpan+trans[1]
    S.dem.par.add(woo.pack.gtsSurface2Facets(surf,wire=False,flex=pre.deformable,mat=meshMat,halfThick=pre.halfThick,fixed=(not pre.deformable)))
    S.dem.par.add(woo.utils.wall(zMin,axis=2,sense=1,mat=meshMat,glAB=((xMin,yMin),(xMax,yMax)),mask=DemField.defaultMovableMask))
    S.dem.saveDead=True # for traces, if used
    
    nan=float('nan')


    if not pre.grid: collider=InsertionSortCollider([Bo1_Sphere_Aabb(),Bo1_Facet_Aabb(),Bo1_Wall_Aabb()],verletDist=0.01)
    else: collider=GridCollider([Grid1_Sphere(),Grid1_Facet(),Grid1_Wall()],label='collider')

    S.engines=woo.dem.DemField.minimalEngines(model=pre.model,grid=pre.grid,dynDtPeriod=100,
    )+[
        woo.core.PyRunner(10,'S.plot.addData(i=S.step,t=S.time,total=S.energy.total(),relErr=(S.energy.relErr() if S.step>100 else 0),**S.energy)'),
        woo.core.PyRunner(50,'import woo.pre.horse\nif S.step>100 and S.energy["kinetic"]<S.pre.relEkStop*abs(S.energy["grav"]): woo.pre.horse.finished(S)'),
    ]
    S.trackEnergy=True
    S.plot.plots={'i':('total','**S.energy'),' t':('relErr')}
    S.plot.data={'i':[nan],'total':[nan],'relErr':[nan]} # to make plot displayable from the very start

    if pre.deformable:
        # set damping just for mesh nodes, if particles have no damping
        if S.lab.leapfrog.damping==0.:
            setNoDamp=True
            S.lab.leapfrog.damping=pre.meshDamping 
        else: setNoDamp=False

        for n in S.dem.nodes:
            DemData.setOriMassInertia(n)
            if type(n.dem.parRef[0].shape)==Membrane:
                if pre.stand and n.pos[2]<-0.22: n.dem.blocked='xyzXYZ'
            else: # non-membrane
                if setNoDamp: n.dem.dampingSkip=True
        for p in S.dem.par:
            if type(p.shape)!=Membrane: continue
            p.mask=(DemField.defaultMovableMask|S.dem.loneMask)^DemField.defaultOutletMask # make horse movable, but avoid deletion by the deleter

        # more complicated here
        # go through facet's nodes, give them some mass
        #nodeM=1e-1*mat.density*(4/3)*math.pi*pre.radius**3
        #nodeI=3e3*(2/5.)*nodeM*pre.radius**2
        #for p in S.dem.par:
        #    if type(p.shape)!=Membrane:
        #        if not setNoDamp: continue
        #        for n in p.shape.nodes:
        #            n.dem.dampingSkip=True
        #    else:
        #        for n in p.shape.nodes:
        #            n.dem.mass=nodeM
        #            n.dem.inertia=nodeI*Vector3(1,1,1)
        #            # n.dem.gravitySkip=True

        S.engines=S.engines[:-1]+[IntraForce([In2_Membrane_ElastMat(bending=True,thickness=(pre.radius if pre.halfThick<=0 else float('nan'))),In2_Sphere_ElastMat()])]+[S.engines[-1]] # put dynDt to the very end
        S.lab.contactLoop.applyForces=False

    if pre.grid:
        c=S.lab.collider
        zMax=max([p.shape.nodes[0].pos[2] for p in S.dem.par])
        c.domain=((xMin,yMin,zMin-.01*zSpan),(xMax,yMax,zMax))
        c.minCellSize=2*pre.radius
        c.verletDist=.5*pre.radius

    S.engines=S.engines+[
        BoxOutlet(box=((xMin,yMin,zMin-.1*zSpan),(xMax,yMax,aabb[1][2]+.1*zSpan)),inside=False,stepPeriod=100),
        VtkExport(out=pre.vtkPrefix,stepPeriod=pre.vtkStep,what=VtkExport.all,dead=(pre.vtkStep<=0 or not pre.vtkPrefix)),
        # VtkFlowExport(out=pre.vtkPrefix+'-flow-',box=((xMin,yMin,zMin-.1*zSpan),(xMax,yMax,aabb[1][2]+.1*zSpan)),stepPeriod=pre.vtkFlowStep,dead=(pre.vtkFlowStep<=0 or not pre.vtkPrefix),stDev=3*pre.radius,divSize=3*pre.radius),
        FlowAnalysis(label='flowAnalysis',box=((xMin,yMin,zMin-.1*zSpan),(xMax,yMax,aabb[1][2]+.1*zSpan)),stepPeriod=pre.vtkFlowStep,cellSize=5*pre.radius,cellData=False,dead=(pre.vtkFlowStep<=0 or not pre.vtkPrefix)),
    ]+(
        [Tracer(stepPeriod=20,num=16,compress=0,compSkip=2,dead=False,scalar=Tracer.scalarVel,label='tracer')] if 'opengl' in woo.config.features else []
    )

    #woo.master.timingEnabled=True
    S.dtSafety=pre.dtSafety
    import woo.config
    if 'opengl' in woo.config.features:
        S.gl(
            woo.gl.Gl1_DemField(shape='spheroids',colorBy='vel',deadNodes=False),
            woo.gl.Gl1_Membrane(relPhi=0.),
        )
    return S

def plotBatchResults(db):
    'Hook called from woo.batch.writeResults'

    import re,math,woo.batch,os
    results=woo.batch.dbReadResults(db)
    out='%s.pdf'%re.sub(r'\.hdf5$','',db)
    from matplotlib.figure import Figure
    from matplotlib.backends.backend_agg import FigureCanvasAgg
    fig=Figure();
    canvas=FigureCanvasAgg(fig)
    ax1=fig.add_subplot(2,1,1)
    ax2=fig.add_subplot(2,1,2)
    ax1.set_xlabel('Time [s]')
    ax1.set_ylabel('Kinetic energy [J]')
    ax1.grid(True)
    ax2.set_xlabel('Time [s]')
    ax2.set_ylabel('Relative energy error')
    ax2.grid(True)
    for res in results:
        series=res['series']
        pre=res['pre']
        if not res['title']: res['title']=res['sceneId']
        ax1.plot(series['t'],series['kinetic'],label=res['title'],alpha=.6)
        ax2.plot(series['t'],series['relErr'],label=res['title'],alpha=.6)
    for ax,loc in (ax1,'lower left'),(ax2,'lower right'):
        l=ax.legend(loc=loc,labelspacing=.2,prop={'size':7})
        l.get_frame().set_alpha(.4)
    fig.savefig(out)
    print('Batch figure saved to file://%s'%os.path.abspath(out))

    

def finished(S):
    import os,re,woo.batch,woo.utils,codecs
    S.stop()
    repName=(os.path.abspath(S.pre.reportFmt.format(S=S,**(dict(S.tags)))) if S.pre.reportFmt else None)
    woo.batch.writeResults(S,defaultDb='horse.hdf5',series=S.plot.data,postHooks=[plotBatchResults],simulationName='horse',report=(('file://'+repName) if repName else None))

    # energy plot, to show how to add plot to the report
    # instead of doing pylab stuff here, just get plot object from woo
    figs=S.plot.plot(noShow=True,subPlots=False)
    
    if repName:
        repExtra=re.sub(r'\.xhtml$','',repName)
        svgs=[]
        for i,fig in enumerate(figs):
            svgs.append(('Figure %d'%i,repExtra+'.fig-%d.svg'%i))
            fig.savefig(svgs[-1][1])

        rep=codecs.open(repName,'w','utf-8','replace')
        html=woo.utils.htmlReportHead(S,'Report for falling horse simulation')+'<h2>Figures</h2>'+(
            u'\n'.join(['<h3>'+svg[0]+u'</h3>'+'<img src="%s" alt="%s"/>'%(os.path.basename(svg[1]),svg[0]) for svg in svgs])
            +'</body></html>'
        )
        rep.write(html)
        rep.close()

        if not woo.batch.inBatch():
            import webbrowser
            webbrowser.open('file://'+os.path.abspath(repName),new=2)

    ## save flow data
    #if not S.lab.flowAnalysis.dead: S.lab.flowAnalysis.vtkExport(out=S.pre.vtkPrefix)

    # catch exception if there are no VTK data (disabled in preprocessor)
    if S.pre.vtkPrefix and (S.pre.vtkStep>0 or S.pre.vtkFlowStep>0):
        try:
            import woo.paraviewscript
            pvscript=woo.paraviewscript.fromEngines(S,out=S.expandTags(S.pre.vtkPrefix),launch=(not woo.batch.inBatch()))
            print('Paraview script written to',pvscript)
        except RuntimeError: raise
