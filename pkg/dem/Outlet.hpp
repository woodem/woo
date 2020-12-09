#pragma once
#include<woo/core/Engine.hpp>
#include<woo/pkg/dem/Particle.hpp>
#include<woo/lib/pyutil/converters.hpp>


#ifdef WOO_OPENGL
	#include<woo/lib/opengl/GLUtils.hpp>
	#include<woo/lib/base/CompUtils.hpp>
#endif

struct Outlet: public PeriodicEngine{
	WOO_DECL_LOGGER;
	bool acceptsField(Field* f) override { return dynamic_cast<DemField*>(f); }
	virtual bool isInside(const Vector3r& p, int& loc) { throw std::runtime_error(pyStr()+" did not override Outlet::isInside."); }
	void run() override;
	py::object pyPsd(bool mass, bool cumulative, bool normalize, int num, const Vector2r& dRange, const Vector2r& tRange, bool zip, bool emptyOk, const py::list& locs__);
	py::object pyDiamMass(bool zipped=false) const;
	py::object pyDiamMassTime(bool zipped=false) const;
	Real pyMassOfDiam(Real min, Real max) const ;
	void pyClear(){ diamMassTime.clear(); rDivR0.clear(); locs.clear(); par.clear(); mass=0.; num=0; }
	#ifdef WOO_OPENGL
		void renderMassAndRate(const Vector3r& pos);
	#endif
	#define woo_dem_Outlet__CLASS_BASE_DOC_ATTRS_PY \
		Outlet,PeriodicEngine,"Delete/mark particles which fall outside (or inside, if *inside* is True) given box. Deleted/mark particles are optionally stored in the *diamMassTime* array for later processing, if needed.\n\nParticle are deleted when :obj:`markMask` is 0, otherwise they are only marked with :obj:`markMask` and not deleted.", \
		((uint,markMask,0,,"When non-zero, switch to marking mode -- particles of which :obj:`Particle.mask` does not comtain :obj:`markMask` (i.e. ``(mask&markMask)!=markMask``) have :obj:`markMask` bit-added to :obj:`Particle.mask` (this can happen only once for each particle); particles are not deleted, but their diameter/mass added to :obj:`diamMassTime` if :obj:`save` is True.")) \
		((uint,mask,((void)":obj:`DemField.defaultOutletMask`",DemField::defaultOutletMask),,"If non-zero, only particles matching the mask will be candidates for removal")) \
		((bool,inside,false,,"Delete particles which fall inside the volume rather than outside")) \
		((bool,save,false,,"Save particle data which are deleted in the :obj:`diamMassTime` list")) \
		((bool,recoverRadius,false,,"Recover radius of Spheres by computing it back from particle's mass and its material density (used when radius is changed due to radius thinning (in Law2_L6Geom_PelletPhys_Pellet.thinningFactor). When radius is recovered, the :math:`r/r_0` ratio is added to :obj:`rDivR0` for further processing.")) \
		((vector<Real>,rDivR0,,AttrTrait<>().noGui().readonly(),"List of the :math:`r/r_0` ratio of deleted particles, when :obj:`recoverRadius` is true.")) \
		((vector<Vector3r>,diamMassTime,,/*must be hidden since pybind11 will not let us re-define it with function of the same name below */AttrTrait<Attr::hidden>(),"Radii and masses of deleted particles; not accessible from python (shadowed by the :obj:`diamMassTime` method).")) \
		((vector<int>,locs,,AttrTrait<>().noGui().readonly(),"Integer location specified for particles; -1 by default, derived classes can use this for any purposes (usually more precise location within the outlet volume).")) \
		((int,num,0,AttrTrait<Attr::readonly>(),"Number of deleted particles")) \
		((bool,savePar,false,,"Save particles as objects in :obj:`par`")) \
		((vector<shared_ptr<Particle>>,par,,AttrTrait<>().noGui().noDump().readonly(),"Deleted :obj:`particles <Particle>` (only saved with :obj:`savePar`.")) \
		((Real,mass,0.,AttrTrait<Attr::readonly>(),"Total mass of deleted particles")) \
		((Real,glColor,0,AttrTrait<>(),"Color for rendering (NaN disables rendering)")) \
		((bool,glHideZero,false,,"Show numbers (mass and rate) even if they are zero.")) \
		((Real,currRate,NaN,AttrTrait<>().readonly(),"Current value of mass flow rate")) \
		((Real,currRateSmooth,1,AttrTrait<>().range(Vector2r(0,1)),"Smoothing factor for currRate ∈〈0,1〉")) \
		((int,kinEnergyIx,-1,AttrTrait<Attr::hidden|Attr::noSave>(),"Index for kinetic energy in scene.energy")) \
		,/*py*/ \
		.def("psd",&Outlet::pyPsd,WOO_PY_ARGS(py::arg("mass")=true,py::arg("cumulative")=true,py::arg("normalize")=true,py::arg("num")=80,py::arg("dRange")=Vector2r(NaN,NaN),py::arg("tRange")=Vector2r(NaN,NaN),py::arg("zip")=false,py::arg("emptyOk")=false,py::arg("locs")=py::list()),"Return particle size distribution of deleted particles (only useful with *save*), spaced between *dRange* (a 2-tuple of minimum and maximum radius); )") \
		.def("clear",&Outlet::pyClear,"Clear information about saved particles (particle list, if saved, mass and number, rDivR0)") \
		.def("diamMass",&Outlet::pyDiamMass,WOO_PY_ARGS(py::arg("zipped")=false),"With *zipped*, return list of (diameter, mass); without *zipped*, return tuple of 2 arrays, diameters and masses.") \
		.def("diamMassTime",&Outlet::pyDiamMassTime,WOO_PY_ARGS(py::arg("zipped")=false),"With *zipped*, return list of (diameter, mass, time); without *zipped*, return tuple of 3 arrays: diameters, masses, times.") \
		.def("massOfDiam",&Outlet::pyMassOfDiam,WOO_PY_ARGS(py::arg("min")=0,py::arg("max")=Inf),"Return mass of particles of which diameters are between *min* and *max*."); \
		woo::converters_cxxVector_pyList_2way<shared_ptr<Outlet>>(mod); // converter needed for DetectSteadyState
		

	WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_Outlet__CLASS_BASE_DOC_ATTRS_PY);
};
WOO_REGISTER_OBJECT(Outlet);


struct BoxOutlet: public Outlet{
	#ifdef WOO_OPENGL
		void render(const GLViewInfo&) override;
	#endif
	bool isInside(const Vector3r& p, int& loc) override { return box.contains(node?node->glob2loc(p):p); }
	#define woo_dem_BoxOutlet__CLASS_BASE_DOC_ATTRS \
		BoxOutlet,Outlet,"Outlet with box geometry", \
		((AlignedBox3r,box,AlignedBox3r(),,"Box volume specification (lower and upper corners). If :obj:`node` is specified, the box is in local coordinates; otherwise, global coorinates are used.")) \
		((shared_ptr<Node>,node,,,"Node specifying local coordinates; if not given :obj:`box` is in global coords."))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_BoxOutlet__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(BoxOutlet);

struct StackedBoxOutlet: public BoxOutlet{
	#ifdef WOO_OPENGL
		void render(const GLViewInfo&) override;
	#endif
	void postLoad(StackedBoxOutlet&, void* attr);
	bool isInside(const Vector3r& p, int& loc) override;
	#define woo_dem_StackedBoxOutlet__CLASS_BASE_DOC_ATTRS \
		StackedBoxOutlet,BoxOutlet,"Box outlet with subdivision along one axis, so that more precise location can be obtained; this is functionally equivalent to multiple adjacent :obj:`BoxOutlet's <BoxOutlet>`, but faster since it is a single engine.", \
		((vector<Real>,divs,,,"Coordinates of division between boxes in the stack; must be an increasing sequence.")) \
		((vector<Real>,divColors,,,"Colors for rendering the dividers; if not given, use darkened :obj:`~woo.dem.BoxOutlet.color`, same for all dividers.")) \
		((short,axis,0,,"Axis along which the :obj:`box` is subdivided.")) \
		((int,loc0,0,,"Index at which numbering of boxes starts. The first box is 0 by default, but it can be changed using this attribute, e.g. setting loc0=10 will make the first box 10, second 11 etc."))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_StackedBoxOutlet__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(StackedBoxOutlet);

struct ArcOutlet: public Outlet{
	bool isInside(const Vector3r& p, int& loc) override;
	void postLoad(ArcOutlet&, void* attr);
	#ifdef WOO_OPENGL
		void render(const GLViewInfo&) override;
	#endif
	#define woo_dem_ArcOutlet__CLASS_BASE_DOC_ATTRS \
		ArcOutlet,Outlet,"Outlet detnig/marking particles in prismatic arc (revolved rectangle) specified using `cylindrical coordinates <http://en.wikipedia.org/wiki/Cylindrical_coordinate_system>`__ (with the ``ISO 31-11`` convention, as mentioned at the Wikipedia page) in a local system. See also analogous :obj:`ArcInlet`.", \
		((shared_ptr<Node>,node,make_shared<Node>(),AttrTrait<Attr::triggerPostLoad>(),"Node defining local coordinates system. *Must* be given.")) \
		((AlignedBox3r,cylBox,,,"Box in cylindrical coordinates, as: (ρ₀,φ₀,z₀),(ρ₁,φ₁,z₁). ρ must be non-negative, (φ₁-φ₀)≤2π.")) \
		((int,glSlices,32,,"Number of slices for rendering circle (the arc takes the proportionate value"))

	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_ArcOutlet__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(ArcOutlet);
