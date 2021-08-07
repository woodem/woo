#pragma once
#include<woo/pkg/dem/Particle.hpp>
#include<woo/pkg/dem/IntraForce.hpp>
#include<woo/pkg/dem/Collision.hpp>
#include<woo/pkg/dem/FrictMat.hpp>
#include<woo/pkg/dem/Sphere.hpp>


struct Cone: public Shape{
	int numNodes() const override { return 2; }
	#define woo_dem_Cone__CLASS_BASE_DOC_ATTRS_CTOR \
		Cone,Shape,"Line element without internal forces, with circular cross-section and hemi-spherical caps at both ends. Geometrically the same :obj:`Capsule`, but with 2 nodes.", \
		((Vector2r,radii,Vector2r(NaN,NaN),,"Radii of the cone. One of them can be zero (for full cone). When both radii are non-zero, the result is conical frustum.")) \
		,/*ctor*/createIndex();
	WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_Cone__CLASS_BASE_DOC_ATTRS_CTOR);
	REGISTER_CLASS_INDEX(Cone,Shape);
};
WOO_REGISTER_OBJECT(Cone);

struct Cg2_Cone_Sphere_L6Geom: public Cg2_Any_Any_L6Geom__Base{
	virtual bool go(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const Vector3r& shift2, const bool& force, const shared_ptr<Contact>& C) override;
	#define woo_dem_Cg2_Cone_Sphere_L6Geom__CLASS_BASE_DOC \
		Cg2_Cone_Sphere_L6Geom,Cg2_Any_Any_L6Geom__Base,"Incrementally compute :obj:`L6Geom` for contact between :obj:`Cone` and :obj:`Sphere`. Uses attributes of :obj:`Cg2_Sphere_Sphere_L6Geom`."
	WOO_DECL__CLASS_BASE_DOC(woo_dem_Cg2_Cone_Sphere_L6Geom__CLASS_BASE_DOC);
	FUNCTOR2D(Cone,Sphere);
	DEFINE_FUNCTOR_ORDER_2D(Cone,Sphere);
	WOO_DECL_LOGGER;
};
WOO_REGISTER_OBJECT(Cg2_Cone_Sphere_L6Geom);

#ifdef WOO_OPENGL
#include<woo/pkg/gl/Functors.hpp>
struct Gl1_Cone: public GlShapeFunctor{
	virtual void go(const shared_ptr<Shape>&, const Vector3r&, bool, const GLViewInfo&) override;
	FUNCTOR1D(Cone);
	#define woo_dem_Gl1_Cone__CLASS_BASE_DOC_ATTRS \
		Gl1_Cone,GlShapeFunctor,"Render cone particles", \
		((int,slices,12,,"Number of slices, controls quality")) \
		((int,stacks,6,,"Number of stacks, controls quality")) \
		((bool,wire,false,,"Render all shapes with wireframe only"))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_Gl1_Cone__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(Gl1_Cone);
#endif

struct Bo1_Cone_Aabb: public BoundFunctor{
	void go(const shared_ptr<Shape>&) override;
	FUNCTOR1D(Cone);
	#define woo_dem_Bo1_Cone_Aabb__CLASS_BASE_DOC \
		Bo1_Cone_Aabb,BoundFunctor,"Compute :obj:`woo.dem.Aabb` of a :obj:`Cone` particle"
	WOO_DECL__CLASS_BASE_DOC(woo_dem_Bo1_Cone_Aabb__CLASS_BASE_DOC);
};
WOO_REGISTER_OBJECT(Bo1_Cone_Aabb);

