#pragma once
#include<woo/pkg/dem/FrictMat.hpp>
#include<woo/pkg/dem/L6Geom.hpp>
#include<woo/pkg/dem/ContactLoop.hpp>


struct LudingMat: public FrictMat{
	#define woo_dem_LudingMat__CLASS_BASE_DOC_ATTRS \
		LudingMat,FrictMat,"Material for :ref:`luding-contact-model`.", \
		((Real,k1DivKn,.5,AttrTrait<>().startGroup("Normal"),"Ratio of plastic (loading) stiffness to maximum normal (elastic, unloading/reloading) stiffness.")) \
		((Real,kaDivKn,.2,,"Ratio of adhesive 'stiffness' to elastic (unloading) stiffness.")) \
		((Real,deltaLimRel,.1,,"Maximum plasticity depth relative to minimum radius of contacting particles.")) \
		((Real,viscN,0.,,"Normal viscous coefficient.")) \
		((Real,dynDivStat,.7,AttrTrait<>().startGroup("Tangential"),"Dynamic to static friction ratio (:math:`\\phi_d=\\mu_d/\\mu_s`); applied in tangential, rolling and twisting senses.")) \
		((Real,viscT,0.,,"Tangential viscosity.")) \
		((Real,krDivKn,.2,AttrTrait<>().startGroup("Rolling"),"Rolling stiffness relative to maximal elastic stiffness (divided by average contact radius for dimensional consistency).")) \
		((Real,statR,.4,,"Rolling static friction coefficient.")) \
		((Real,viscR,.1,,"Rolling viscous coefficient for rolling.")) \
		((Real,kwDivKn,.2,AttrTrait<>().startGroup("Twisting"),"Twist stiffness relative to maximal elastic stiffness (divided by average contact radius for dimensional consistency).")) \
		((Real,statW,.3,,"Twisting static friction coefficient.")) \
		((Real,viscW,.1,,"Twisting viscous coefficient.")) 

	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_LudingMat__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(LudingMat);

struct LudingMatState: public MatState{
	size_t getNumScalars() const override { return 3; }
	string getScalarName(int index) override{
		switch(index){
			case 0: return "total";
			case 1: return "viscous";
			case 2: return "plastic";
			default: return "";
		};
	}
	Real getScalar(int index, const long& step, const Real& smooth=0) override{
		switch(index){
			case 0: return total;
			case 1: return visco;
			case 2: return plast;
			default: return NaN;
		};
	}
	#define woo_dem_LudingMatState__CLASS_BASE_DOC_ATTRS \
		LudingMatState,MatState,"Hold detailed per-particle data such as dissipated energy, for the Luding model (:ref:`luding-contact-model`.)", \
		((Real,total,0.,,"Total dissipated energy (sum of the terms below)")) \
		((Real,visco,0.,,"Energy dissipated by viscous effects.")) \
		((Real,plast,0.,,"Energy dissipated by plastic effects."))

	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_LudingMatState__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(LudingMatState);


struct LudingPhys: public FrictPhys{
	enum{WORK_PLASTIC=0,WORK_VISCOUS=1,WORK_SIZE=2};
	Real Wplast()const{ return work.size()==WORK_SIZE?work[WORK_PLASTIC]:NaN; }
	Real Wvisc()const{ return work.size()==WORK_SIZE?work[WORK_VISCOUS]:NaN; }
	#define woo_dem_LudingPhys__CLASS_BASE_DOC_ATTRS_PY \
		LudingPhys,FrictPhys,"Physical properties for :ref:`luding-contact-model`.", \
			/* FrictPhys: kn≡k2 (as current elastic stiffness), kt, tanPhi */ \
			((Real,kn1,NaN,,"Normal plastic (loading) stiffness."))\
			((Real,kna,NaN,,"Normal adhesive stiffness.")) \
			((Real,k2hat,NaN,,"Maximum stiffness.")) \
			((Real,deltaMax,0.,,"Historically maximum overlap value.")) \
			((Real,deltaLim,NaN,,"Upper limit for :obj:`deltaMax`.")) \
			((Real,kr,NaN,,"Roll stiffness.")) \
			((Real,kw,NaN,,"Twist stiffness.")) \
			((Real,viscN,NaN,,"Normal viscosity.")) \
			((Real,viscT,NaN,,"Tangent viscosity.")) \
			((Real,viscR,NaN,,"Roll viscosity.")) \
			((Real,viscW,NaN,,"Twist viscosity.")) \
			((Real,dynDivStat,NaN,,"Dynamic to static friction.")) \
			((Real,statR,NaN,,"Roll static friction.")) \
			((Real,statW,NaN,,"Twist static friction.")) \
			((Vector2r,xiT,Vector2r::Zero(),,"Tangent elastic displacement.")) \
			((Vector2r,xiR,Vector2r::Zero(),,"Tangent elastic rotation.")) \
			((Real,xiW,0.,,"Twist elastic rotation.")) \
			((vector<Real>,work,,,"Per-contact dissipation (unallocated when energy tracking is not enabled).")) \
			,/*py*/ .def_readonly("Wplast",&LudingPhys::Wplast).def_readonly("Wvisc",&LudingPhys::Wvisc)

	WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_LudingPhys__CLASS_BASE_DOC_ATTRS_PY);
};
WOO_REGISTER_OBJECT(LudingPhys);


struct Cp2_LudingMat_LudingPhys: public Cp2_FrictMat_FrictPhys{
	void go(const shared_ptr<Material>&, const shared_ptr<Material>&, const shared_ptr<Contact>&) override;
	FUNCTOR2D(LudingMat,LudingMat);
	#define woo_dem_Cp2_LudingMat_LudingPhys__CLASS_BASE_DOC_ATTRS \
		Cp2_LudingMat_LudingPhys,Cp2_FrictMat_FrictPhys,"Compute :obj:`LudingPhys` given two instances of :obj:`LudingMat`.",/*attrs*/
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_Cp2_LudingMat_LudingPhys__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(Cp2_LudingMat_LudingPhys);

struct Law2_L6Geom_LudingPhys: public LawFunctor{
	bool go(const shared_ptr<CGeom>&, const shared_ptr<CPhys>&, const shared_ptr<Contact>&) override;
	void addWork(LudingPhys& ph, int type, const Real& dW, const Vector3r& xyz);
	void commitWork(const shared_ptr<Contact>& C);

	FUNCTOR2D(L6Geom,LudingPhys);
	WOO_DECL_LOGGER;
	#define woo_dem_Law2_L6Geom_LudingPhys__CLASS_BASE_DOC_ATTRS \
		Law2_L6Geom_LudingPhys,LawFunctor,"Contact law implementing :ref:`luding-contact-model`.", \
		((bool,wImmediate,true,,"Increment plastic & viscous work in :obj:`S.energy <woo.core.Scene.EnergyTracker>` in every step rather than commiting the sum when contact dissolves. Note that :obj:`Particle.matState <woo.dem.MatState>` (if a :obj:`LudingMatState`) is always updated only when the contact dissolves.")) \
		((int,viscIx,-1,AttrTrait<Attr::noSave|Attr::hidden>(),"Index of viscous dissipation.")) \
		((int,plastIx,-1,AttrTrait<Attr::noSave|Attr::hidden>(),"Index of plastic dissipation."))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_Law2_L6Geom_LudingPhys__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(Law2_L6Geom_LudingPhys);


