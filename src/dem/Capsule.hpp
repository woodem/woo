// © 2014 Václav Šmilauer <eu@doxos.eu>
#pragma once

#include"Particle.hpp"
#include"L6Geom.hpp"
#include"Sphere.hpp"
#include"Wall.hpp"
#include"InfCylinder.hpp"
#include"Facet.hpp"


struct Capsule: public Shape{
	void selfTest(const shared_ptr<Particle>&) override;
	int numNodes() const override { return 1; }
	void setFromRaw(const Vector3r& _center, const Real& _radius, vector<shared_ptr<Node>>& nn, const vector<Real>& raw) override;
	void asRaw(Vector3r& _center, Real& _radius, vector<shared_ptr<Node>>&nn, vector<Real>& raw) const override;
	// recompute inertia and mass
	void lumpMassInertia(const shared_ptr<Node>& n, Real density, Real& mass, Matrix3r& I, bool& rotateOk) override;
	Real equivRadius() const override;
	Real volume() const override;
	bool isInside(const Vector3r& pt) const override;
	// compute axis-aligned bounding box
	AlignedBox3r alignedBox() const override;
	Vector3r endPt(short i) const { return nodes[0]->loc2glob(Vector3r((i==0?-.5:.5)*shaft,0,0)); }
	void applyScale(Real scale) override;
	#define woo_dem_Capsule__CLASS_BASE_DOC_ATTRS_CTOR_PY \
		Capsule,Shape,"Cylinder with half-spherical caps on both sides, Mindowski sum of segment with sphere.", \
		((Real,radius,NaN,AttrTrait<>().lenUnit(),"Radius of the capsule -- of half-spherical caps and also of the middle part.")) \
		((Real,shaft,NaN,AttrTrait<>().lenUnit(),"Length of the middle segment")) \
		,/*ctor*/createIndex(); \
		,/*py*/ .def("endPt",&Capsule::endPt,py::arg("i"),"Return one of capsule endpoints. The first (negative on local :math:`x`-axis) is returned with *i=0, otherwise the second one is returned.")

	WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR_PY(woo_dem_Capsule__CLASS_BASE_DOC_ATTRS_CTOR_PY);
	REGISTER_CLASS_INDEX(Capsule,Shape);
};
WOO_REGISTER_OBJECT(Capsule);

struct Bo1_Capsule_Aabb: public BoundFunctor{
	virtual void go(const shared_ptr<Shape>&) override;
	FUNCTOR1D(Capsule);
	#define woo_dem_Bo1_Capsule_Aabb__CLASS_BASE_DOC \
		Bo1_Capsule_Aabb,BoundFunctor,"Creates/updates an :obj:`Aabb` of a :obj:`Capsule`"
	WOO_DECL__CLASS_BASE_DOC(woo_dem_Bo1_Capsule_Aabb__CLASS_BASE_DOC);
};
WOO_REGISTER_OBJECT(Bo1_Capsule_Aabb);


struct Cg2_Capsule_Capsule_L6Geom: public Cg2_Any_Any_L6Geom__Base{
	bool go(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const Vector3r& shift2, const bool& force, const shared_ptr<Contact>& C) override;
	void setMinDist00Sq(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const shared_ptr<Contact>& C) override;
	#define woo_dem_Cg2_Capsule_Capsule_L6Geom__CLASS_BASE_DOC \
		Cg2_Capsule_Capsule_L6Geom,Cg2_Any_Any_L6Geom__Base,"Collision of two :obj:`Capsule` shapes."
	WOO_DECL__CLASS_BASE_DOC(woo_dem_Cg2_Capsule_Capsule_L6Geom__CLASS_BASE_DOC);
	FUNCTOR2D(Capsule,Capsule);
	DEFINE_FUNCTOR_ORDER_2D(Capsule,Capsule);
	//WOO_DECL_LOGGER;
};
WOO_REGISTER_OBJECT(Cg2_Capsule_Capsule_L6Geom);

struct Cg2_Sphere_Capsule_L6Geom: public Cg2_Any_Any_L6Geom__Base{
	bool go(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const Vector3r& shift2, const bool& force, const shared_ptr<Contact>& C) override;
	void setMinDist00Sq(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const shared_ptr<Contact>& C) override;
	#define woo_dem_Cg2_Sphere_Capsule_L6Geom__CLASS_BASE_DOC \
		Cg2_Sphere_Capsule_L6Geom,Cg2_Any_Any_L6Geom__Base,"Compute :obj:`L6Geom` for contact of :obj:`~woo.dem.Capsule` and :obj:`~woo.dem.Sphere`."
	WOO_DECL__CLASS_BASE_DOC(woo_dem_Cg2_Sphere_Capsule_L6Geom__CLASS_BASE_DOC);
	FUNCTOR2D(Sphere,Capsule);
	DEFINE_FUNCTOR_ORDER_2D(Sphere,Capsule);
};
WOO_REGISTER_OBJECT(Cg2_Sphere_Capsule_L6Geom);

struct Cg2_Wall_Capsule_L6Geom: public Cg2_Any_Any_L6Geom__Base{
	bool go(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const Vector3r& shift2, const bool& force, const shared_ptr<Contact>& C) override;
	#define woo_dem_Cg2_Wall_Capsule_L6Geom__CLASS_BASE_DOC \
		Cg2_Wall_Capsule_L6Geom,Cg2_Any_Any_L6Geom__Base,"Compute :obj:`L6Geom` for contact of :obj:`~woo.dem.Capsule` and :obj:`~woo.dem.Wall` (axis-aligned plane)."
	WOO_DECL__CLASS_BASE_DOC(woo_dem_Cg2_Wall_Capsule_L6Geom__CLASS_BASE_DOC);
	FUNCTOR2D(Wall,Capsule);
	DEFINE_FUNCTOR_ORDER_2D(Wall,Capsule);
};
WOO_REGISTER_OBJECT(Cg2_Wall_Capsule_L6Geom);

struct Cg2_InfCylinder_Capsule_L6Geom: public Cg2_Any_Any_L6Geom__Base{
	bool go(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const Vector3r& shift2, const bool& force, const shared_ptr<Contact>& C) override;
	#define woo_dem_Cg2_InfCylinder_Capsule_L6Geom__CLASS_BASE_DOC \
		Cg2_InfCylinder_Capsule_L6Geom,Cg2_Any_Any_L6Geom__Base,"Compute :obj:`L6Geom` for contact of :obj:`~woo.dem.Capsule` and :obj:`~woo.dem.InfCylinder`."
	WOO_DECL__CLASS_BASE_DOC(woo_dem_Cg2_InfCylinder_Capsule_L6Geom__CLASS_BASE_DOC);
	FUNCTOR2D(InfCylinder,Capsule);
	DEFINE_FUNCTOR_ORDER_2D(InfCylinder,Capsule);
};
WOO_REGISTER_OBJECT(Cg2_InfCylinder_Capsule_L6Geom);

struct Cg2_Facet_Capsule_L6Geom: public Cg2_Any_Any_L6Geom__Base{
	WOO_DECL_LOGGER;
	bool go(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const Vector3r& shift2, const bool& force, const shared_ptr<Contact>& C) override;
	#define woo_dem_Cg2_Facet_Capsule_L6Geom__CLASS_BASE_DOC \
		Cg2_Facet_Capsule_L6Geom,Cg2_Any_Any_L6Geom__Base,"Compute :obj:`L6Geom` for contact of :obj:`~woo.dem.Capsule` and :obj:`~woo.dem.Facet`."
	WOO_DECL__CLASS_BASE_DOC(woo_dem_Cg2_Facet_Capsule_L6Geom__CLASS_BASE_DOC);
	FUNCTOR2D(Facet,Capsule);
	DEFINE_FUNCTOR_ORDER_2D(Facet,Capsule);
};
WOO_REGISTER_OBJECT(Cg2_Facet_Capsule_L6Geom);


#ifdef WOO_OPENGL
#include"../gl/Functors.hpp"
struct Gl1_Capsule: public Gl1_Sphere{
	virtual void go(const shared_ptr<Shape>& shape, const Vector3r& shift, bool wire2,const GLViewInfo& glInfo) override;
	#define woo_dem_Gl1_Capsule__CLASS_BASE_DOC \
		Gl1_Capsule,Gl1_Sphere,"Renders :obj:`woo.dem.Capsule` object"
	WOO_DECL__CLASS_BASE_DOC(woo_dem_Gl1_Capsule__CLASS_BASE_DOC);
	RENDERS(Capsule);
};
WOO_REGISTER_OBJECT(Gl1_Capsule);
#endif

