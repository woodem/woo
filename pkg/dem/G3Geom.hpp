#pragma once
#include<woo/pkg/dem/Particle.hpp>
#include<woo/pkg/dem/ContactLoop.hpp>
#include<woo/pkg/dem/Sphere.hpp>
#include<woo/pkg/dem/Wall.hpp>

struct G3Geom: public CGeom{
	// rotates any contact-local vector expressed inglobal coordinates to keep track of local contact rotation in last step
	void rotateVectorWithContact(Vector3r& v);
	static Vector3r getIncidentVel(const DemData& dd1, const DemData& dd2, Real dt, const Vector3r& shift2, const Vector3r& shiftVel2, bool avoidGranularRatcheting, bool useAlpha);
	#define woo_dem_G3Geom__CLASS_BASE_DOC_ATTRS_CTOR \
		G3Geom,CGeom,"Geometry of particles in contact, defining relative velocities.", \
		((Real,uN,NaN,,"Normal displacement, distace of separation of particles (mathematically equal to integral of vel[0], but given here for numerically more stable results, as this component can be usually computed directly).")) \
		((Vector3r,dShear,Vector3r::Zero(),,"Shear displacement delta during last step.")) \
		((Vector3r,twistAxis,Vector3r(NaN,NaN,NaN),AttrTrait<Attr::readonly>(),"Axis of twisting rotation")) \
		((Vector3r,orthonormalAxis,Vector3r(NaN,NaN,NaN),AttrTrait<Attr::readonly>(),"Axis normal to twisting axis")) \
		((Vector3r,normal,Vector3r(NaN,NaN,NaN),AttrTrait<Attr::readonly>(),"Contact normal in global coordinates; G3Geom doesn't touch Contact.node.ori (which is identity), therefore orientation must be kep separately")) \
		, /*ctor*/ createIndex();
	WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_G3Geom__CLASS_BASE_DOC_ATTRS_CTOR);
	REGISTER_CLASS_INDEX(G3Geom,CGeom);
};
WOO_REGISTER_OBJECT(G3Geom);

struct Cg2_Sphere_Sphere_G3Geom: public CGeomFunctor{
	virtual bool go(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const Vector3r& shift2, const bool& force, const shared_ptr<Contact>& C) override;
	#define woo_dem_Cg2_Sphere_Sphere_G3Geom__CLASS_BASE_DOC_ATTRS \
		Cg2_Sphere_Sphere_G3Geom,CGeomFunctor,"Incrementally compute :obj:`G3Geom` for contact of 2 spheres. Detailed documentation in py/_extraDocs.py", \
		((bool,noRatch,true,,"FIXME: document what it really does.")) \
		((bool,useAlpha,true,,"Use alpha correction proposed by McNamara, see source code for details"))

	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_Cg2_Sphere_Sphere_G3Geom__CLASS_BASE_DOC_ATTRS);
	FUNCTOR2D(Sphere,Sphere);
	DEFINE_FUNCTOR_ORDER_2D(Sphere,Sphere);
	WOO_DECL_LOGGER;
};
WOO_REGISTER_OBJECT(Cg2_Sphere_Sphere_G3Geom);

struct Cg2_Wall_Sphere_G3Geom: public CGeomFunctor{
	virtual bool go(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const Vector3r& shift2, const bool& force, const shared_ptr<Contact>& C) override;
	#define woo_dem_Cg2_Wall_Sphere_G3Geom__CLASS_BASE_DOC \
		Cg2_Wall_Sphere_G3Geom,CGeomFunctor,"Incrementally compute :obj:`G3Geom` for contact of 2 spheres. Detailed documentation in py/_extraDocs.py"
	WOO_DECL__CLASS_BASE_DOC(woo_dem_Cg2_Wall_Sphere_G3Geom__CLASS_BASE_DOC);
	FUNCTOR2D(Wall,Sphere);
	DEFINE_FUNCTOR_ORDER_2D(Wall,Sphere);
	WOO_DECL_LOGGER;
};
WOO_REGISTER_OBJECT(Cg2_Wall_Sphere_G3Geom);


struct G3GeomCData: public CData{
	#define woo_dem_G3GeomCData__CLASS_BASE_DOC_ATTRS \
		G3GeomCData,CData,"Internal variables for use with G3Geom", \
		((Vector3r,shearForce,Vector3r::Zero(),,"Shear force in global coordinates"))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_G3GeomCData__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(G3GeomCData);

struct Law2_G3Geom_FrictPhys_IdealElPl: public LawFunctor{
	bool go(const shared_ptr<CGeom>&, const shared_ptr<CPhys>&, const shared_ptr<Contact>&) override;
	FUNCTOR2D(G3Geom,FrictPhys);
	#define woo_dem_Law2_G3Geom_FrictPhys_IdealElPl__CLASS_BASE_DOC_ATTRS \
		Law2_G3Geom_FrictPhys_IdealElPl,LawFunctor,"Ideally elastic-plastic behavior, for use with G3Geom.", \
		((bool,noSlip,false,,"Disable plastic slipping")) \
		((bool,noBreak,false,,"Disable removal of contacts when in tension.")) \
		((int,plastDissipIx,-1,AttrTrait<Attr::noSave|Attr::hidden>(),"Index of plastically dissipated energy")) \
		((int,elastPotIx,-1,AttrTrait<Attr::hidden|Attr::noSave>(),"Index for elastic potential energy"))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_Law2_G3Geom_FrictPhys_IdealElPl__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(Law2_G3Geom_FrictPhys_IdealElPl);

