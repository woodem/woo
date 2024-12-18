#pragma once
#ifdef WOO_OPENGL
//#include"Particle.hpp"
#include"../gl/Functors.hpp"


GL_FUNCTOR(GlShapeFunctor,TYPELIST_4(const shared_ptr<Shape>&, /*shift*/ const Vector3r&, /*wire*/bool,const GLViewInfo&),Shape);
GL_DISPATCHER(GlShapeDispatcher,GlShapeFunctor);

//GL_FUNCTOR(GlCGeomFunctor,TYPELIST_3(const shared_ptr<CGeom>&, const shared_ptr<Contact>& c, /*wire*/ bool),CGeom);
//GL_DISPATCHER(GlCGeomDispatcher,GlCGeomFunctor);

GL_FUNCTOR(GlBoundFunctor,TYPELIST_1(const shared_ptr<Bound>&),Bound);
GL_DISPATCHER(GlBoundDispatcher,GlBoundFunctor);

GL_FUNCTOR(GlCPhysFunctor,TYPELIST_3(const shared_ptr<CPhys>&, const shared_ptr<Contact>&, const GLViewInfo&),CPhys);
GL_DISPATCHER(GlCPhysDispatcher,GlCPhysFunctor);


struct Gl1_DemField: public GlFieldFunctor{
	virtual void go(const shared_ptr<Field>&, GLViewInfo*) override;
	GLViewInfo* viewInfo; // set when called, so that it does not have to be passed around
	shared_ptr<DemField> dem; // used by do* methods
	void doShape();
	void doBound();
	void doNodes(const vector<shared_ptr<Node>>& nodeContainer);
	void doContactNodes();
	void doCPhys();
	void postLoad(Gl1_DemField&, void* attr);
	//; static void setSceneRange(Scene* scene, const shared_ptr<ScalarRange>& prev, const shared_ptr<ScalarRange>& curr);
	// static void setOurSceneRanges(Scene* scene, const vector<shared_ptr<ScalarRange>>& ours, const list<shared_ptr<ScalarRange>>& used);
	void initAllRanges();


	void setFunctors_getRanges(const vector<shared_ptr<Object>>& ff, vector<shared_ptr<ScalarRange>>& rr) override;


	Vector3r getNodeVel(const shared_ptr<Node>& n) const;
	Vector3r getParticleVel(const shared_ptr<Particle>& n) const;
	Vector3r getNodeAngVel(const shared_ptr<Node>& n) const;

	enum{COLOR_SOLID=0,COLOR_SHAPE,COLOR_RADIUS,COLOR_DIAM_MM,COLOR_VEL,COLOR_ANGVEL,COLOR_MASS,COLOR_POS,COLOR_DISPLACEMENT,COLOR_ROTATION,COLOR_REFPOS,COLOR_MAT_ID,COLOR_MATSTATE,COLOR_SIG_N,COLOR_SIG_T,COLOR_MASK,COLOR_NUM_CON,COLOR_FLAGS,COLOR_INVISIBLE,/*last*/COLOR_SENTINEL};
	enum{GLYPH_KEEP=0,GLYPH_NONE,GLYPH_FORCE,GLYPH_TORQUE,GLYPH_VEL,GLYPH_ANGVEL,/*last*/GLYPH_SENTINEL};
	enum{CNODE_NONE=0,CNODE_GLREP=1,CNODE_LINE=2,CNODE_NODE=4,CNODE_POTLINE=8};
	enum{SHAPE_NONE=0,SHAPE_ALL,SHAPE_SPHERES,SHAPE_NONSPHERES,SHAPE_MASK};
	enum{AXIS_X=0,AXIS_Y,AXIS_Z,AXIS_YZ,AXIS_ZX,AXIS_XY,AXIS_NORM};

	RENDERS(DemField);
	WOO_DECL_LOGGER;

	
	#define GL1_DEMFIELD_COLORBY_NAMEDENUM {{COLOR_SOLID,{"solid"}},{COLOR_SHAPE,{"Shape.color","shape"}},{COLOR_RADIUS,{"radius","r"}},{COLOR_DIAM_MM,{"diameter (mm)","diameter","diam","d"}},{COLOR_VEL,{"velocity","vel","v"}},{COLOR_ANGVEL,{"angular velocity","angVel"}},{COLOR_MASS,{"mass","m"}},{COLOR_MASK,{"mask"}},{COLOR_POS,{"position","pos"}},{COLOR_DISPLACEMENT,{"ref. displacement","disp","displacement"}},{COLOR_ROTATION,{"ref. rotation","rot","rotation"}},{COLOR_REFPOS,{"refpos coord","refpos"}},{COLOR_MAT_ID,{"material id","mat id"}},{COLOR_MATSTATE,{"Particle.matState","mat state"}},{COLOR_SIG_N,{"normal stress","sigN"}},{COLOR_SIG_T,{"shear stress","sigT"}},{COLOR_NUM_CON,{"number of contacts","num contacts","numCon"}},{COLOR_FLAGS,{"flags"}},{COLOR_INVISIBLE,{"invisible","-"}}}

	#define woo_gl_Gl1_DemField__CLASS_BASE_DOC_ATTRS_CTOR_PY \
		Gl1_DemField,GlFieldFunctor,"Render DEM field.", \
		((int,shape,SHAPE_ALL,AttrTrait<Attr::triggerPostLoad|Attr::namedEnum>().namedEnum({{SHAPE_NONE,{"none",""}},{SHAPE_ALL,{"all"}},{SHAPE_SPHERES,{"spheroids","sph"}},{SHAPE_NONSPHERES,{"non-spheroids","nsph"}},{SHAPE_MASK,{"mask"}}}).startGroup("Shape"),"Render only particles matching selected filter.")) \
		((uint,mask,0,,"Only shapes/bounds of particles with this group will be shown; 0 matches all particles.")) \
		((bool,shape2,true,AttrTrait<Attr::triggerPostLoad>(),"Render also particles not matching :obj:`shape` (using :obj:`colorBy2`)")) \
		((Vector2i,modulo,Vector2i(0,0),,"For particles matching :obj:`shape`, only show particles with :obj:`Particle.id` such that ``(id+modulo[1])%modulo[0]==0`` (similar to :obj:`woo.dem.Tracer.modulo`). Only nodes of which first particle matches (or don't have any particle attached) are shown (in case of nodes, regardless of its :obj:`shape`). Display of contacts is not affected by this value.")) \
		((bool,wire,false,,"Render all shapes with wire only")) \
		\
		((int,colorBy,COLOR_SHAPE,AttrTrait<Attr::triggerPostLoad|Attr::namedEnum>().namedEnum(GL1_DEMFIELD_COLORBY_NAMEDENUM).buttons({"Reference now","S.renderer.setRefNow=True","use current positions and orientations as reference for showing displacement/rotation"},/*showBefore*/false),"Color particles by")) \
		((int,vecAxis,AXIS_NORM,AttrTrait<Attr::namedEnum>().namedEnum({{AXIS_X,{"x"}},{AXIS_Y,{"y"}},{AXIS_Z,{"z"}},{AXIS_YZ,{"yz","yz"}},{AXIS_ZX,{"zx","xz"}},{AXIS_XY,{"xy","yx"}},{AXIS_NORM,{"norm","magnitude","xyz"}}}).hideIf("self.colorBy in ('Shape.color', 'radius', 'diameter (mm)', 'material id', 'Particle.matState', 'number of contacts')"),"Axis for vector quantities.")) \
		((int,matStateIx,0,AttrTrait<Attr::triggerPostLoad>().hideIf("self.colorBy!='Particle.matState'  and self.colorBy2!='Particle.matState'"),"Index for getting :obj:`MatState` scalars.")) \
		((Real,matStateSmooth,1e-3,AttrTrait<>().hideIf("self.colorBy!='Particle.matState' and self.colorBy2!='Particle.matState'"),"Smoothing coefficient for :obj:`MatState` scalars.")) \
		((shared_ptr<ScalarRange>,colorRange,,AttrTrait<>().readonly().hideIf("self.colorBy in ('solid','invisible')"),"Range for particle colors (:obj:`colorBy`)")) \
		\
		((int,colorBy2,COLOR_SOLID,AttrTrait<Attr::triggerPostLoad|Attr::namedEnum>().namedEnum(GL1_DEMFIELD_COLORBY_NAMEDENUM).hideIf("not self.shape2"),"Color for particles with :obj:`shape2`.")) \
		((shared_ptr<ScalarRange>,colorRange2,,AttrTrait<>().readonly().hideIf("not self.shape2 or self.colorBy2 in ('solid','invisible')"),"Range for particle colors (:obj:`colorBy`)")) \
		((Vector3r,solidColor,Vector3r(.3,.3,.3),AttrTrait<>().rgbColor(),"Solid color for particles.")) \
		\
		((vector<shared_ptr<ScalarRange>>,colorRanges,,AttrTrait<>().readonly().noGui(),"List of color ranges")) \
		\
		((bool,bound,false,,"Render particle's :obj:`Bound`")) \
		((bool,periodic,false,AttrTrait<>().noGui(),"Automatically shows whether the scene is periodic (to use in hideIf of :obj:`fluct`")) \
		((bool,fluct,false,AttrTrait<>().hideIf("not self.periodic or (self.colorBy not in ('velocity','angular velocity') and self.glyph not in ('velocity',))"),"With periodic boundaries, show only fluctuation components of velocity.")) \
		\
		((bool,nodes,false,AttrTrait<>().startGroup("Nodes"),"Render DEM nodes")) \
		((int,glyph,GLYPH_KEEP,AttrTrait<Attr::triggerPostLoad|Attr::namedEnum>().namedEnum({{GLYPH_KEEP,{"keep"}},{GLYPH_NONE,{"none",""}},{GLYPH_FORCE,{"force","f"}},{GLYPH_TORQUE,{"torque","t"}},{GLYPH_VEL,{"velocity","vel","v"}},{GLYPH_ANGVEL,{"angular velocity","angVel","angvel"}}}),"Show glyphs on particles by setting :obj:`GlData` on their nodes.")) \
		((shared_ptr<ScalarRange>,glyphRange,,AttrTrait<>().readonly(),"Range for glyph colors")) \
		((Real,glyphRelSz,.1,,"Maximum glyph size relative to scene radius")) \
		((bool,deadNodes,true,,"Show :obj:`DemField.deadNodes <woo.dem.DemField.deadNodes>`.")) \
		((vector<shared_ptr<ScalarRange>>,glyphRanges,,AttrTrait<>().readonly().noGui(),"List of glyph ranges")) \
		\
		((int,cNode,CNODE_NONE,AttrTrait<>().bits({"glRep","line","node","potLine"}).startGroup("Contact nodes"),"What should be shown for contact nodes")) \
		((bool,cPhys,false,,"Render contact's nodes")) \
 		\
		((int,guiEvery,100,,"Process GUI events once every *guiEvery* objects are painted, to keep the ui responsive. Set to 0 to make rendering blocking.")) \
		\
		((shared_ptr<GlShapeDispatcher>,shapeDispatcher,make_shared<GlShapeDispatcher>(),AttrTrait<>().readonly().noGui(),"Dispatcher for rendering :obj:`shapes <woo.dem.Shape>`. Set up automatically.")) \
		((shared_ptr<GlBoundDispatcher>,boundDispatcher,make_shared<GlBoundDispatcher>(),AttrTrait<>().readonly().noGui(),"Dispatcher for rendering :obj:`bounds <woo.dem.Bound>`. Set up automatically.")) \
		((shared_ptr<GlCPhysDispatcher>,cPhysDispatcher,make_shared<GlCPhysDispatcher>(),AttrTrait<>().readonly().noGui(),"Dispatcher for rendering :obj:`CPhys <woo.dem.CPhys>`. Set up automatically.")) \
		, /*ctor*/ initAllRanges(); \
		, /*py*/ \
			; \
			_classObj.attr("glyphKeep")=(int)Gl1_DemField::GLYPH_KEEP; \
			_classObj.attr("glyphNone")=(int)Gl1_DemField::GLYPH_NONE; \
			_classObj.attr("glyphForce")=(int)Gl1_DemField::GLYPH_FORCE; \
			_classObj.attr("glyphTorque")=(int)Gl1_DemField::GLYPH_TORQUE; \
			_classObj.attr("glyphVel")=(int)Gl1_DemField::GLYPH_VEL; \
			_classObj.attr("glyphAngVel")=(int)Gl1_DemField::GLYPH_ANGVEL; \
			_classObj.attr("shapeAll")=(int)Gl1_DemField::SHAPE_ALL; \
			_classObj.attr("shapeNone")=(int)Gl1_DemField::SHAPE_NONE; \
			_classObj.attr("shapeSpheres")=(int)Gl1_DemField::SHAPE_SPHERES; \
			_classObj.attr("shapeNonSpheres")=(int)Gl1_DemField::SHAPE_NONSPHERES; \
			_classObj.attr("shapeMask")=(int)Gl1_DemField::SHAPE_MASK; \
			_classObj.attr("colorSolid")=(int)Gl1_DemField::COLOR_SOLID; \
			_classObj.attr("colorShape")=(int)Gl1_DemField::COLOR_SHAPE; \
			_classObj.attr("colorRadius")=(int)Gl1_DemField::COLOR_RADIUS; \
			_classObj.attr("colorDiamMM")=(int)Gl1_DemField::COLOR_DIAM_MM; \
			_classObj.attr("colorMass")=(int)Gl1_DemField::COLOR_MASS; \
			_classObj.attr("colorMask")=(int)Gl1_DemField::COLOR_MASK; \
			_classObj.attr("colorVel")=(int)Gl1_DemField::COLOR_VEL; \
			_classObj.attr("colorAngVel")=(int)Gl1_DemField::COLOR_ANGVEL; \
			_classObj.attr("colorPos")=(int)Gl1_DemField::COLOR_POS; \
			_classObj.attr("colorDisplacement")=(int)Gl1_DemField::COLOR_DISPLACEMENT; \
			_classObj.attr("colorRotation")=(int)Gl1_DemField::COLOR_ROTATION; \
			_classObj.attr("colorRefPos")=(int)Gl1_DemField::COLOR_REFPOS; \
			_classObj.attr("colorMatId")=(int)Gl1_DemField::COLOR_MAT_ID; \
			_classObj.attr("colorMatState")=(int)Gl1_DemField::COLOR_MATSTATE; \
			_classObj.attr("colorSigN")=(int)Gl1_DemField::COLOR_SIG_N; \
			_classObj.attr("colorSigT")=(int)Gl1_DemField::COLOR_SIG_T; \
			_classObj.attr("colorNumCon")=(int)Gl1_DemField::COLOR_NUM_CON; \
			_classObj.attr("colorInvisible")=(int)Gl1_DemField::COLOR_INVISIBLE; \
			_classObj.attr("cNodeNone")=(int)Gl1_DemField::CNODE_NONE; \
			_classObj.attr("cNodeGlRep")=(int)Gl1_DemField::CNODE_GLREP; \
			_classObj.attr("cNodeLine")=(int)Gl1_DemField::CNODE_LINE; \
			_classObj.attr("cNodeNode")=(int)Gl1_DemField::CNODE_NODE; \
			_classObj.attr("cNodePotLine")=(int)Gl1_DemField::CNODE_POTLINE;

	WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR_PY(woo_gl_Gl1_DemField__CLASS_BASE_DOC_ATTRS_CTOR_PY);
};
WOO_REGISTER_OBJECT(Gl1_DemField);
#endif
