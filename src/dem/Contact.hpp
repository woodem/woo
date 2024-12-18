/* all declarations for contacts are in the woo/pkg/dem/Particle.hpp header:
	declarations contain boost::python code, which need full declaration,
	not just forwards.
*/
#pragma once

#include"../core/Scene.hpp"
#include"Particle.hpp" // for Particle::id_t
#include"../supp/pyutil/converters.hpp"


struct Particle;
struct Scene;
struct Node;


struct CGeom: public Object,public Indexable{
	// XXX: is createIndex() called here at all??
	#define woo_dem_CGeom__CLASS_BASE_DOC_ATTRS_CTOR_PY \
		CGeom,Object,ClassTrait().doc("Geometrical configuration of contact").section("Geometry","TODO",{"CGeomFunctor","CGeomDispatcher"}), \
		((shared_ptr<Node>,node,make_shared<Node>(),,"Local coordinates definition.")) \
		,/*ctor*/ createIndex(); ,/*py*/WOO_PY_TOPINDEXABLE(CGeom);
	WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR_PY(woo_dem_CGeom__CLASS_BASE_DOC_ATTRS_CTOR_PY);
	REGISTER_INDEX_COUNTER(CGeom);
};
WOO_REGISTER_OBJECT(CGeom);

struct CPhys: public Object, public Indexable{
	#define woo_dem_CPhys__CLASS_BASE_DOC_ATTRS_CTOR_PY \
		CPhys,Object,ClassTrait().doc("Physical properties of contact.").section("Physical properties","TODO",{"CPhysFunctor","CPhysDispatcher"}), \
		/*attrs*/ \
		((Vector3r,force,Vector3r::Zero(),AttrTrait<>().forceUnit(),"Force applied on the first particle in the contact")) \
		((Vector3r,torque,Vector3r::Zero(),AttrTrait<>().torqueUnit(),"Torque applied on the first particle in the contact")) \
		,/*ctor*/ createIndex(); ,/*py*/WOO_PY_TOPINDEXABLE(CPhys)
	WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR_PY(woo_dem_CPhys__CLASS_BASE_DOC_ATTRS_CTOR_PY);
	REGISTER_INDEX_COUNTER(CPhys);
};
WOO_REGISTER_OBJECT(CPhys);

struct CData: public Object{
	#define woo_dem_CData__CLASS_BASE_DOC CData,Object,ClassTrait().doc("Optional data stored in the contact by the Law functor.").section("Contact law","TODO",{"LawFunctor","LawDispatcher","LawTester"})

	WOO_DECL__CLASS_BASE_DOC(woo_dem_CData__CLASS_BASE_DOC);
};
WOO_REGISTER_OBJECT(CData);

struct Contact: public Object{
	bool isReal() const { return geom&&phys; }
	bool isColliding() const { return stepCreated>=0; }
	void setNotColliding(){ stepCreated=-1; }
	bool isFresh(Scene* s){ return s->step==stepCreated; }
	bool pyIsFresh(shared_ptr<Scene> s){ return s->step==stepCreated; }
	void swapOrder();
	void reset();
	// return -1 or +1 depending on whether the particle passed to us is pA or pB
	int forceSign(const shared_ptr<Particle>& p) const { return p.get()==leakPA()?1:-1; }
	// python version with parameter correctness checks
	int pyForceSign(const shared_ptr<Particle>& p) const;
	int pyForceSignId(const Particle::id_t& p) const;
	// return 0 or 1 depending on whether the particle passed is pA or pB
	short pIndex(const shared_ptr<Particle>& p) const { return pIndex(p.get()); }
	short pIndex(const Particle* p) const { return p==leakPA()?0:1; }
	/* return force and torque in global coordinates which act at contact point C located at branch from node nodeI of particle.
	Contact force is reversed automatically if particle==pB.
	Force and torque at node itself are  F and branch.cross(F)+T respectively.
	Branch takes periodicity (cellDist) in account.
	See In2_Sphere_Elastmat::go for its use.  */
	std::tuple<Vector3r,Vector3r,Vector3r> getForceTorqueBranch(const shared_ptr<Particle>& p, int nodeI, Scene* scene){ return getForceTorqueBranch(p.get(),nodeI,scene); }
	std::tuple<Vector3r,Vector3r,Vector3r> getForceTorqueBranch(const Particle*, int nodeI, Scene* scene);
	// return position vector between pA and pB, taking in account PBC's; both must be uninodal
	Vector3r dPos(const Scene* s) const;
	Vector3r dPos_py() const{ if(pA.expired()||pB.expired()) return Vector3r(NaN,NaN,NaN); return dPos(Master::instance().getScene().get()); }
	Real dist_py() const { return dPos_py().norm(); }
	Particle::id_t pyId1() const;
	Particle::id_t pyId2() const;
	Vector2i pyIds() const;
	void pyResetPhys(){ phys.reset(); }
	virtual string pyStr() const override { return "<Contact ##"+to_string(pyId1())+"+"+to_string(pyId2())+" @ "+ptr_to_string(this)+">"; }
	// potentially unsafe pointer extraction (assert safety in debug builds only)
	// only use when the particles are sure to exist and never return this pointer anywhere
	// we call it "leak" to make this very explicit
	Particle* leakPA() const { assert(!pA.expired()); return pA.lock().get(); }
	Particle* leakPB() const { assert(!pB.expired()); return pB.lock().get(); }
	Particle* leakOther(const Particle* p) const { assert(p==leakPA() || p==leakPB()); return (p!=leakPA()?leakPA():leakPB()); }
	shared_ptr<Particle> pyPA() const { return pA.lock(); }
	shared_ptr<Particle> pyPB() const { return pB.lock(); }
	#ifdef WOO_OPENGL
		#define woo_dem_Contact__OPENGL__color ((Real,color,0,,"(Normalized) color value for this contact"))
	#else
		#define woo_dem_Contact__OPENGL__color
	#endif

	#define woo_dem_Contact__CLASS_BASE_DOC_ATTRS_PY \
		Contact,Object,ClassTrait().doc("Contact in DEM").section("Contacts","TODO",{"ContactLoop","ContactHook","CGeom","CPhys","CData"}), \
		((shared_ptr<CGeom>,geom,,AttrTrait<Attr::readonly>(),"Contact geometry")) \
		((shared_ptr<CPhys>,phys,,AttrTrait<Attr::readonly>(),"Physical properties of contact")) \
		((shared_ptr<CData>,data,,AttrTrait<Attr::readonly>(),"Optional data stored by the functor for its own use")) \
		/* these two are overridden below because of weak_ptr */  \
		((weak_ptr<Particle>,pA,,AttrTrait<Attr::readonly>(),"First particle of the contact")) \
		((weak_ptr<Particle>,pB,,AttrTrait<Attr::readonly>(),"Second particle of the contact")) \
		((Vector3i,cellDist,Vector3i::Zero(),AttrTrait<Attr::readonly>(),"Distace in the number of periodic cells by which pB must be shifted to get to the right relative position.")) \
		woo_dem_Contact__OPENGL__color \
		((int,stepCreated,-1,AttrTrait<Attr::readonly>(),"Step in which this contact was created by the collider, or step in which it was made real (if geom and phys exist). This number is NOT reset by Contact::reset(). If negative, it means the collider does not want to keep this contact around anymore (this happens if the contact is real but there is no overlap anymore).")) \
		((Real,minDist00Sq,-1,AttrTrait<Attr::readonly>(),"Minimum distance between nodes[0] of both shapes so that the contact can exist. Set in ContactLoop by geometry functor once, and is used to check for possible contact without having to call the functor. If negative, not used. Currently, only Sphere-Sphere contacts use this information.")) \
		((int,stepLastSeen,-1,AttrTrait<Attr::readonly>(),"")) \
		((size_t,linIx,0,AttrTrait<Attr::readonly>().noGui(),"Position in the linear view (ContactContainer)")) \
		, /*py*/ .add_property_readonly("id1",&Contact::pyId1,":obj:`Particle.id` of the first contacting particle.").add_property_readonly("id2",&Contact::pyId2,":obj:`Particle.id` of the second contacting particle.").add_property_readonly("real",&Contact::isReal,"Whether the contact is real (has :obj:`geom` and :obj:`phys`); unreal contacts are created by broadband collisions detection and have no physical significance.").add_property_readonly("ids",&Contact::pyIds,":obj:`IDs <Particle.id>` of both contacting particles as 2-tuple.") \
		.def("dPos",&Contact::dPos_py,"Return position difference vector pB-pA, taking `Contact.cellDist` in account properly. Both particles must be uninodal, exception is raised otherwise.") \
		.def("dist",&Contact::dist_py,"Shorthand for dPos.norm().") \
		.add_property_readonly("pA",&Contact::pyPA,"First particle of the contact") \
		.add_property_readonly("pB",&Contact::pyPB,"Second particle of the contact") \
		.def("resetPhys",&Contact::pyResetPhys,"Set :obj:`phys` to *None* (to force its re-evaluation)") \
		.def("isFresh",&Contact::pyIsFresh,WOO_PY_ARGS(py::arg("scene")),"Say whether this contact has just been created. Equivalent to ``C.stepCreated==scene.step``.") \
		.def("forceSign",&Contact::pyForceSign,WOO_PY_ARGS(py::arg("p")),"Return sign of :obj:`CPhys.force` as it appies on the particle passed, i.e. +1 if ``p==C.pA`` and -1 if ``p==C.pB``. Raise an exception if ``p`` is neither ``pA`` or ``pB``.") \
		.def("forceSign",&Contact::pyForceSignId,WOO_PY_ARGS(py::arg("id")),"Return sign of :obj:`CPhys.force` as it appies on the particle with id ``id``, i.e. ``id==C.id1`` and -1 if ``id==id2``. Raise an exception if ``id`` is neither ``id1`` or ``id2``.") \
		; \
		woo::converters_cxxVector_pyList_2way<shared_ptr<Contact>>(mod);

	WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_Contact__CLASS_BASE_DOC_ATTRS_PY);
};
WOO_REGISTER_OBJECT(Contact);


