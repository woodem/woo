#pragma once
#include"../core/Dispatcher.hpp"
#include"Particle.hpp"
#include"Inlet.hpp"
#include"../supp/pyutil/converters.hpp"


#ifdef WOO_OPENGL
// functor types
#include"Gl1_DemField.hpp"
#endif

struct Aabb: public Bound{
	#define woo_dem_Aabb__CLASS_BASE_DOC_ATTRS_CTOR \
		Aabb,Bound,"Axis-aligned bounding box, for use with `InsertionSortCollider`.", \
		((vector<Vector3r>,nodeLastPos,,AttrTrait<>(Attr::readonly).lenUnit(),"Node positions when bbox was last updated.")) \
		((Real,maxD2,0,AttrTrait<>(Attr::readonly).unit("m²").noGui(),"Maximum allowed squared distance for nodal displacements (i.e. how much was the bbox enlarged last time)")) \
		((Real,maxRot,NaN,AttrTrait<>(Attr::readonly),"Maximum allowed rotation (in radians, without discriminating different angles) that does not yet invalidate the bbox. Functor sets to -1 (or other negative value) for particles where node rotation does not influence the box (such as spheres or facets); in that case, orientation difference is not computed at all. If it is left at NaN, it is an indication that the functor does not implemnt this behavior and an error will be raised in the collider.")) \
		((vector<Quaternionr>,nodeLastOri,,AttrTrait<>(Attr::readonly),"Node orientations when bbox was last updated.")) \
		, /*ctor*/ createIndex();
	
	WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_Aabb__CLASS_BASE_DOC_ATTRS_CTOR);
	REGISTER_CLASS_INDEX(Aabb,Bound);
};
WOO_REGISTER_OBJECT(Aabb);

#ifdef WOO_OPENGL
struct Gl1_Aabb: public GlBoundFunctor{
	virtual void go(const shared_ptr<Bound>&) override;
	RENDERS(Aabb);
	#define woo_dem_Gl1_Aabb__CLASS_BASE_DOC Gl1_Aabb,GlBoundFunctor,"Render Axis-aligned bounding box (:obj:`woo.dem.Aabb`)."
	WOO_DECL__CLASS_BASE_DOC(woo_dem_Gl1_Aabb__CLASS_BASE_DOC);
};
WOO_REGISTER_OBJECT(Gl1_Aabb);
#endif


struct BoundFunctor: public Functor1D</*dispatch types*/ Shape,/*return type*/ void, /*argument types*/ TYPELIST_1(const shared_ptr<Shape>&)>{
	#define woo_dem_BoundFunctor__CLASS_BASE_DOC_PY BoundFunctor,Functor,"Functor for creating/updating :obj:`woo.dem.Bound`.",/*py*/; woo::converters_cxxVector_pyList_2way<shared_ptr<BoundFunctor>>(mod);
	WOO_DECL__CLASS_BASE_DOC_PY(woo_dem_BoundFunctor__CLASS_BASE_DOC_PY);
};
WOO_REGISTER_OBJECT(BoundFunctor);

struct BoundDispatcher: public Dispatcher1D</* functor type*/ BoundFunctor>{
	void run() override;
	WOO_DISPATCHER1D_FUNCTOR_DOC_ATTRS_CTOR_PY(BoundDispatcher,BoundFunctor,/*optional doc*/,
		/*additional attrs*/
		,/*ctor*/,/*py*/
	);
};
WOO_REGISTER_OBJECT(BoundDispatcher);


struct Collider: public Engine{
	public:
		/*! Probe the Aabb on particle's presence. Returns list of body ids with which there is potential overlap. */
		virtual vector<Particle::id_t> probeAabb(const Vector3r& mn, const Vector3r& mx){throw std::runtime_error("Calling abstract Collider.probeAabb.");}

		/*!
		Tell whether given bodies may interact, for other than spatial reasons.
		
		Concrete collider implementations should call this function if the bodies are in potential interaction geometrically. */
		static bool mayCollide(const DemField*, const shared_ptr<Particle>&, const shared_ptr<Particle>&);
		/*!
		Invalidate all persistent data (if the collider has any), forcing reinitialization at next run.
		The default implementation does nothing, colliders should override it if it is applicable.
		*/
		virtual void invalidatePersistentData(){}
		// ctor with functors for the integrated BoundDispatcher
	/* \n\n.. admonition:: Special constructor\n\n\tDerived colliders (unless they override ``pyHandleCustomCtorArgs``) can be given list of :obj:`BoundFunctors <BoundFunctor>` which is used to initialize the internal :obj:`boundDispatcher <Collider.boundDispatcher>` instance. */
	#define woo_dem_Collider__CLASS_BASE_DOC_ATTRS_PY \
		Collider,Engine,ClassTrait().doc("Abstract class for finding spatial collisions between bodies.").section("Collision detection","TODO",{"Bound","BoundFunctor","GridBoundFunctor","BoundDispatcher","GridBoundDispatcher"}) \
		,/*attrs*/ \
			((int,nFullRuns,0,AttrTrait<>(),"Cumulative number of full runs, when collision detection is needed.")) \
		,/*py*/ .def("probeAabb",&Collider::probeAabb,py::arg("mn"),py::arg("mx"),"Return list of particles intersected by axis-aligned box with given corners") \
		.def_static("mayCollide",&Collider::mayCollide,py::arg("dem"),py::arg("pA"),py::arg("pB"),"Predicate whether two particles in question may collide or not")

	WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_Collider__CLASS_BASE_DOC_ATTRS_PY);
};
WOO_REGISTER_OBJECT(Collider);

struct AabbCollider: public Collider{
	WOO_DECL_LOGGER;
	void pyHandleCustomCtorArgs(py::args_& t, py::kwargs& d) override;
	void getLabeledObjects(const shared_ptr<LabelMapper>&) override;
	void setVerletDist(Scene* scene, DemField* dem);
	bool aabbIsDirty(const shared_ptr<Particle>& p);
	void updateAabb(const shared_ptr<Particle>& p);
	void initBoundDispatcher();
	#define woo_dem_AabbCollider__CLASS_BASE_DOC_ATTRS \
		AabbCollider,Collider,ClassTrait().doc("Abstract class for colliders based on axis-aligned bounding boxes.")\
		,/*attrs*/ \
		((shared_ptr<BoundDispatcher>,boundDispatcher,make_shared<BoundDispatcher>(),AttrTrait<Attr::readonly>(),":obj:`BoundDispatcher` object that is used for creating :obj:`bounds <Particle.bound>` on collider's request as necessary.")) \
		((Real,verletDist,((void)"Automatically initialized",-.05),,"Length by which to enlarge particle bounds, to avoid running collider at every step. Stride disabled if zero, and bounding boxes are updated at every step. Negative value will trigger automatic computation, so that the real value will be ``|verletDist|`` × minimum spherical particle radius and minimum :obj:`Inlet` radius (for particles which don't exist yet); if there is no minimum radius found, it will be set to 0.0 (with a warning) and disabled.")) \
		((bool,noBoundOk,false,,"Allow particles without bounding box. This is currently only useful for testing :obj:`woo.fem.Tetra` which don't undergo any collisions.")) \

	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_AabbCollider__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(AabbCollider);

