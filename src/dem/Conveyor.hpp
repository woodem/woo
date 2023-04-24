#pragma once
#include"Inlet.hpp"
#include"Clump.hpp"


struct InletMatState: public MatState{
	size_t getNumScalars() const override { return 1; }
	string getScalarName(int index) override {
		switch(index){
			case 0: return "time new";
			case 1: return "age";
			default: return "";
		}
	}
	Real getScalar(int index, const Real& time, const long& step, const Real& smooth=0) override {
		switch(index){
			case 0: return timeNew;
			case 1: return time-timeNew;
			default: return NaN;	
		}
	}
	#define woo_dem_InletMatState__CLASS_BASE_DOC_ATTRS \
		InletMatState,MatState,"Hold dissipated energy data for this particles, to evaluate wear.", \
		((Real,timeNew,-1,,"Step at which this particle was created by the inlet."))

	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_InletMatState__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(InletMatState);

struct ConveyorInlet: public Inlet{
	WOO_DECL_LOGGER;
	bool acceptsField(Field* f) override { return dynamic_cast<DemField*>(f); }
	Real packVol() const;
	void sortPacking(const Real& zTrimVol=-1);
	void postLoad(ConveyorInlet&,void*);
	void notifyDead() override;
	Real critDt() override; 
	void nodeLeavesBarrier(const shared_ptr<Node>& p);
	void setAttachedParticlesColor(const shared_ptr<Node>& n, Real c);
	void setAttachedParticlesMatState(const shared_ptr<Node>& n, const shared_ptr<InletMatState>& ms);
	Vector2r minMaxDiam() const override;

	#ifdef WOO_OPENGL
		void render(const GLViewInfo&) override{
			if(isnan(glColor)) return;
			Inlet::renderMassAndRate(node->pos);
		}
	#endif
	void run() override;
	vector<shared_ptr<Node>> pyBarrier() const { return vector<shared_ptr<Node>>(barrier.begin(),barrier.end()); }
	void pyClear(){ mass=0; num=0; genDiamMassTime.clear(); }
	bool hasClumps(){ return !clumps.empty(); }
	py::object pyDiamMass(bool zipped=false) const;
	Real pyMassOfDiam(Real min, Real max) const ;
	py::object pyPsd(bool mass, bool cumulative, bool normalize, const Vector2r& dRange, const Vector2r& tRange, int num) const;
	#define woo_dem_ConveyorInlet__CLASS_BASE_DOC_ATTRS_PY \
		ConveyorInlet,Inlet,"Inlet producing infinite band of particles from packing periodic in the x-direction. Clumps are fully supported.", \
		((shared_ptr<Material>,material,,AttrTrait<>().startGroup("Particles"),"Material for new particles")) \
		((shared_ptr<SpherePack>,spherePack,,AttrTrait<Attr::noSave|Attr::triggerPostLoad>().noGui(),":obj:`woo.pack.SpherePack` object; when specified, :obj:`centers`, :obj:`radii` (and :obj:`clumps`, if clumps are contained) are discarded  and will be computed from this :obj:`SpherePack`. The attribute is reset afterwards.")) \
		((shared_ptr<ShapePack>,shapePack,,AttrTrait<Attr::triggerPostLoad>().noDump(),"Purely geomerical description of particles to be generated (will replace :obj:`spherePack`, :obj:`centers`, :obj:`radii`, :obj:`clumps` and :obj:`cellLen` in the future).")) \
		((bool,zTrim,false,AttrTrait<>().noGui(),"Trim packing from above so that the ratio of :obj:`vel` / :obj:`packVel` is as small as possible. Spheres/clumps will be discarded from above and this flag will be set to false once trimming is done (it will not be called again explicitly even if :obj:`massRate` or :obj:`vel` change.")) \
		((Real,zTrimHt,NaN,AttrTrait<>().noGui(),"Height at which the packing was trimmed if :obj:`zTrim` was set.")) \
		((Real,cellLen,NaN,AttrTrait<>().lenUnit(),"Length of the band cell, which is repeated periodically (if :obj:`spherePack` is given and is periodic, this value is deduced)")) \
		((vector<Real>,radii,,AttrTrait<Attr::triggerPostLoad>().noGui().noDump(),"Radii for the packing (if :obj:`spherePack` is given, radii are computed)")) \
		((vector<Vector3r>,centers,,AttrTrait<Attr::triggerPostLoad>().noGui().noDump(),"Centers of spheres/clumps in the packing (if :obj:`spherePack` is given, centers are computed)")) \
		((vector<shared_ptr<SphereClumpGeom>>,clumps,,AttrTrait<Attr::triggerPostLoad>().noGui().noDump(),"Clump geometry, corresponding to each :obj:`radii` and :obj:`centers`. (if :obj:`spherePack` is given, clumps are computed)")) \
		((Real,massRate,NaN,AttrTrait<Attr::triggerPostLoad>().massRateUnit(),"Average mass flow rate; if given, :obj:`vel` is adjusted (if both are given, :obj:`massRate` takes precedence).")) \
		((Real,vel,NaN,AttrTrait<Attr::triggerPostLoad>().velUnit(),"Velocity of particles; if specified, :obj:`massRate` is adjusted (of both are given, such as in constructor, :obj:`massRate` has precedence and a warning is issued if the two don't match)")) \
		((Real,packVel,NaN,AttrTrait<>().readonly().velUnit(),"Velocity by which the packing is traversed and new particles emmited; always smaller than or equal to :obj:`vel`. Computed automatically.")) \
		((Real,relLatVel,0.,,"Relative velocity components lateral to :obj:`vel` (local x-axis); both components are assigned with uniform probability from range `(-relLatVel*vel,+relLatVel*vel)`, at the moment the particle leaves the barrier layer.")) \
		\
		((vector<Real>,clipX,,AttrTrait<>().startGroup("Clipping").lenUnit(),"If given, clip the given packing from above by the given function $z_max(x)$ given as piecewise-linear function in same-length arrays :obj:`clipX` and :obj:`clipZ`. If :obj:`clipX` is empty, no clipping is done.")) \
		((vector<Real>,clipZ,,AttrTrait<>().lenUnit(),"Z-coordinate (max sphere $z$ coordinate), with points corresponding to :obj:`clipX`.")) \
		((Real,clipLastX,0,AttrTrait<>().readonly(),"X-coordinate of last-generated particles, for use with clipping (clipping may have different periodicity than the packing, this value can be different from :obj:`lastX` and wraps around :obj:`cellLen`.")) \
		((size_t,clipPos,0,AttrTrait<>().readonly().noGui(),"Internal variable for optimizing interpolation in :obj:`clipX` and :obj:`clipZ`.")) \
		\
		((Real,startLen,NaN,AttrTrait<>().startGroup("Tunables"),"Band to be created at the very start; if NaN, only the usual starting amount is created (depending on feed velocity)")) \
		((Real,barrierColor,.2,,"Color for barrier particles (NaN for random)")) \
		((Real,color,NaN,,"Color for non-barrier particles (NaN for random)")) \
		((Real,barrierLayer,-3.,,"Some length of the last part of new particles has all DoFs blocked, so that when new particles are created, there are no sudden contacts in the band; in the next step, DoFs in this layer are unblocked. If *barrierLayer* is negative, it is relative to the maximum radius in the given packing, and is automatically set to the correct value at the first run")) \
		((Real,movingBedZ,NaN,,"If given, particles with z coordinate lower than this value will move indefinitely with the conveyor (contact velocity, blocked DOFs), technically not added to the barrier at all.")) \
		((Real,movingBedColor,.5,,"Color for particles selected with :obj:`movingBedZ` (NaN for random).")) \
		((bool,save,true,,"Save generated particles so that PSD can be generated afterwards")) \
		((bool,matState,false,,"Endow new particles with :obj:`InletMatState` (for tracking when a particular particle was created and similar.")) \
		\
		((int,nextIx,-1,AttrTrait<>().readonly().startGroup("Bookkeeping"),"Index of last-generated particles in the packing")) \
		((Real,lastX,0,AttrTrait<>().readonly(),"X-coordinate of last-generated particles in the packing")) \
		((list<shared_ptr<Node>>,barrier,,/*exposed via pyBarrier, hide here*/AttrTrait<Attr::hidden>(),"Nodes which make up the barrier and will be unblocked once they leave barrierLayer.")) \
		((shared_ptr<Node>,node,make_shared<Node>(),AttrTrait<>(),"Position and orientation of the factory; local x-axis is the feed direction.")) \
		/* movable beginning */ \
		((shared_ptr<Node>,streamBeginNode,,AttrTrait<>(),"Node at which the particle stream will start, which can be moving during the simulation without disrupting stream continuity. It is the position of this node, projected onto :math:`x`-axis of :obj:`node`, which is relevant.")) \
		((shared_ptr<Impose>,barrierImpose,,AttrTrait<>(),"Imposition which is used for barrier nodes, as long as they are in the barrier.")) \
		((Real,xDebt,0,AttrTrait<>().readonly().noGui(),"Length of packing which was not covered in the last step due to :obj:`streamBeginNode` offset.")) \
		/* end movable beginning */ \
		((int,initSortLimit,200,,"Limit of number of particles generated in a single step above which :obj:`woo.dem.InsertionSortCollider.forceInitSort` will be set (only useful if :obj:`InsertionSortCollider` is actually used).")) \
		((Real,avgRate,NaN,AttrTrait<>().readonly().massRateUnit(),"Average feed rate (computed from :obj:`Material density <Material.density>`, packing and  and :obj:`vel`")) \
		((int,kinEnergyIx,-1,AttrTrait<Attr::hidden|Attr::noSave>(),"Index for kinetic energy in scene.energy")) \
		((vector<Vector3r>,genDiamMassTime,,AttrTrait<Attr::readonly>().noDump().noGui(),"List of generated diameters, masses and times (for making granulometry)")) \
		,/*py*/ \
			.def("barrier",&ConveyorInlet::pyBarrier) \
			.def("clear",&ConveyorInlet::pyClear) \
			.def("diamMass",&ConveyorInlet::pyDiamMass,WOO_PY_ARGS(py::arg("zipped")=false),"Return masses and diameters of generated particles. With *zipped*, return list of (diameter, mass); without *zipped*, return tuple of 2 arrays, diameters and masses.") \
			.def("massOfDiam",&ConveyorInlet::pyMassOfDiam,WOO_PY_ARGS(py::arg("min")=0,py::arg("max")=Inf),"Return mass of particles of which diameters are between *min* and *max*.") \
			.def("psd",&ConveyorInlet::pyPsd,WOO_PY_ARGS(py::arg("mass")=true,py::arg("cumulative")=true,py::arg("normalize")=false,py::arg("dRange")=Vector2r(NaN,NaN),py::arg("tRange")=Vector2r(NaN,NaN),py::arg("num")=80),"Return PSD for particles generated.")
	WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_ConveyorInlet__CLASS_BASE_DOC_ATTRS_PY);
};
WOO_REGISTER_OBJECT(ConveyorInlet);

