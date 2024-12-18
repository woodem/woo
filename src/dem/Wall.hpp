// © 2009 Václav Šmilauer <eudoxos@arcig.cz>
#pragma once
#include"Particle.hpp"
#include"Collision.hpp"
#include"FrictMat.hpp"
#include"IntraForce.hpp"
#include"Sphere.hpp"
#include"Facet.hpp"




/*! Object representing infinite plane aligned with the coordinate system (axis-aligned wall). */
struct Wall: public Shape{
	int numNodes() const override { return 1; }
	void lumpMassInertia(const shared_ptr<Node>&, Real density, Real& mass, Matrix3r& I, bool& rotateOk) override;
	#define woo_dem_Wall__CLASS_BASE_DOC_ATTRS_CTOR \
		Wall,Shape,"Object representing infinite plane aligned with the coordinate system (axis-aligned wall).", \
		((int,sense,0,,"Which side of the wall interacts: -1 for negative only, 0 for both, +1 for positive only.")) \
		((int,axis,0,,"Axis of the normal; can be 0,1,2 for +x, +y, +z respectively (Node's orientation is disregarded for walls)")) \
		((AlignedBox2r,glAB,AlignedBox2r(Vector2r(NaN,NaN),Vector2r(NaN,NaN)),,"Points between which the wall is drawn (if NaN, computed automatically to cover the visible part of the scene)")) \
		,/*ctor*/createIndex();

	WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_Wall__CLASS_BASE_DOC_ATTRS_CTOR);
	REGISTER_CLASS_INDEX(Wall,Shape);
};	
WOO_REGISTER_OBJECT(Wall);



struct Bo1_Wall_Aabb: public BoundFunctor{
	virtual void go(const shared_ptr<Shape>&) override;
	FUNCTOR1D(Wall);
	#define woo_dem_Bo1_Wall_Aabb__CLASS_BASE_DOC_ATTRS Bo1_Wall_Aabb,BoundFunctor,"Creates/updates an :obj:`Aabb` of a :obj:`Wall`", \
		((Real,halfThick,0.,,"Additional thickness added to the Aabb, to work around potential collision detection issues."))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_Bo1_Wall_Aabb__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(Bo1_Wall_Aabb);

struct In2_Wall_ElastMat: public IntraFunctor{
	void go(const shared_ptr<Shape>&, const shared_ptr<Material>&, const shared_ptr<Particle>&) override;
	FUNCTOR2D(Wall,ElastMat);
	#define woo_dem_In2_Wall_ElastMat__CLASS_BASE_DOC In2_Wall_ElastMat,IntraFunctor,"Apply contact forces on wall. Wall generates no internal forces as such. Torque from applied forces is discarded, as Wall does not rotate."
	WOO_DECL__CLASS_BASE_DOC(woo_dem_In2_Wall_ElastMat__CLASS_BASE_DOC);
};
WOO_REGISTER_OBJECT(In2_Wall_ElastMat);

struct Cg2_Wall_Sphere_L6Geom: public Cg2_Any_Any_L6Geom__Base{
	virtual bool go(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const Vector3r& shift2, const bool& force, const shared_ptr<Contact>& C) override;
	virtual bool goReverse(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const Vector3r& shift2, const bool& force, const shared_ptr<Contact>& C) override { throw std::logic_error("ContactLoop should swap interaction arguments, should be Wall+Sphere, but is "+s1->getClassName()+"+"+s2->getClassName()); }
	void setMinDist00Sq(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const shared_ptr<Contact>& C) override { C->minDist00Sq=-1; }
	#define woo_dem_Cg2_Wall_Sphere_L6Geom__CLASS_BASE_DOC \
		Cg2_Wall_Sphere_L6Geom,Cg2_Any_Any_L6Geom__Base,"Incrementally compute :obj:`L6Geom` for contact between :obj:`Wall` and :obj:`Sphere`. Uses attributes of :obj:`Cg2_Sphere_Sphere_L6Geom`."
	WOO_DECL__CLASS_BASE_DOC(woo_dem_Cg2_Wall_Sphere_L6Geom__CLASS_BASE_DOC);
	FUNCTOR2D(Wall,Sphere);
	DEFINE_FUNCTOR_ORDER_2D(Wall,Sphere);
	WOO_DECL_LOGGER;
};
WOO_REGISTER_OBJECT(Cg2_Wall_Sphere_L6Geom);


struct Cg2_Wall_Facet_L6Geom: public Cg2_Any_Any_L6Geom__Base{
	virtual bool go(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const Vector3r& shift2, const bool& force, const shared_ptr<Contact>& C) override;
	virtual bool goReverse(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const Vector3r& shift2, const bool& force, const shared_ptr<Contact>& C) override { throw std::logic_error("ContactLoop should swap interaction arguments, should be Wall+Facet, but is "+s1->getClassName()+"+"+s2->getClassName()); }
	void setMinDist00Sq(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const shared_ptr<Contact>& C) override { C->minDist00Sq=-1; }
	#define woo_dem_Cg2_Wall_Facet_L6Geom__CLASS_BASE_DOC \
		Cg2_Wall_Facet_L6Geom,Cg2_Any_Any_L6Geom__Base,"Incrementally compute :obj:`L6Geom` for contact between :obj:`Wall` and :obj:`Facet`."
	WOO_DECL__CLASS_BASE_DOC(woo_dem_Cg2_Wall_Facet_L6Geom__CLASS_BASE_DOC);
	FUNCTOR2D(Wall,Facet);
	DEFINE_FUNCTOR_ORDER_2D(Wall,Facet);
	WOO_DECL_LOGGER;
};
WOO_REGISTER_OBJECT(Cg2_Wall_Facet_L6Geom);

#ifdef WOO_OPENGL

#include"../gl/Functors.hpp"
struct Gl1_Wall: public GlShapeFunctor{	
	virtual void go(const shared_ptr<Shape>&, const Vector3r&, bool,const GLViewInfo&) override;
	WOO_DECL_LOGGER;
	#define woo_dem_Gl1_Wall__CLASS_BASE_DOC_ATTRS \
		Gl1_Wall,GlShapeFunctor,"Renders :obj:`woo.dem.Wall` object", \
		((int,div,20,,"Number of divisions of the wall inside visible scene part."))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_Gl1_Wall__CLASS_BASE_DOC_ATTRS);
	RENDERS(Wall);
};
WOO_REGISTER_OBJECT(Gl1_Wall);

#endif

