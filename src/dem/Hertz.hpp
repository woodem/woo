#pragma once
#include"FrictMat.hpp"
#include"L6Geom.hpp"

class HertzMat: public FrictMat{
	#define woo_dem_HertzMat__CLASS_BASE_DOC_ATTRS_CTOR \
		HertzMat,FrictMat,"Material parameters to be used with adhesive (Schwarz) model.", \
			((Real,surfEnergy,0.0,,"Surface energy parameter [J/m^2] per each unit contact surface, to derive adhesive (DMT, JKR, Schwarz) formulation from HM. If zero, adhesion is disabled.")) \
			((Real,alpha,0.,,"Parameter interpolating between DMT and JKR extremes in the Schwarz model. :math:`alpha` was introduced in :cite:`Carpick1999`")) \
			, /*ctor*/ createIndex();
	WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_HertzMat__CLASS_BASE_DOC_ATTRS_CTOR);
	REGISTER_CLASS_INDEX(HertzMat,FrictMat);
};
WOO_REGISTER_OBJECT(HertzMat);

class HertzPhys: public FrictPhys{
	#define woo_dem_HertzPhys__CLASS_BASE_DOC_ATTRS_CTOR \
		HertzPhys,FrictPhys,"Physical properties of a contact of two :obj:`FrictMat` with viscous damping enabled (viscosity is currently not provided as material parameter).", \
		/* ((Real,kn0,0,,"Constant for computing current normal stiffness.")) */ \
		((Real,kt0,0,,"Constant for computing current normal stiffness.")) \
		((Real,alpha_sqrtMK,0,,"Value for computing damping coefficient -- see :cite:`Antypov2011`, eq (10).")) \
		((Real,R,0,,"Effective radius (for the Schwarz model)")) \
		((Real,K,0,,"Effective stiffness (for the Schwarz model)")) \
		((Real,gamma,0,,"Surface energy (for the Schwarz model)")) \
		((Real,alpha,0.,,"COS alpha coefficient")) \
		((Real,contRad,0,,"Contact radius, used for storing previous value as the initial guess in the next step.")) \
		/* ((Real,Lc,NaN,,"COS critical load")) ((Real,a0,NaN,,"COS zero load")) */ \
		, /*ctor*/ createIndex();
	WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_HertzPhys__CLASS_BASE_DOC_ATTRS_CTOR);
	REGISTER_CLASS_INDEX(HertzPhys,CPhys);
};
WOO_REGISTER_OBJECT(HertzPhys);

#if 0
struct Cp2_FrictMat_HertzPhys: public Cp2_FrictMat_FrictPhys{
	void go(const shared_ptr<Material>&, const shared_ptr<Material>&, const shared_ptr<Contact>&) override;
	void go_real(const shared_ptr<Material>&, const shared_ptr<Material>&, const shared_ptr<Contact>&);
	FUNCTOR2D(FrictMat,FrictMat);
	WOO_DECL_LOGGER;
	#define woo_dem_Cp2_FrictMat_HertzPhys__CLASS_BASE_DOC_ATTRS \
		Cp2_FrictMat_HertzPhys,Cp2_FrictMat_FrictPhys,"Compute :obj:`HertzPhys` given two instances of :ref`FrictMat`. \n\n.. note: This functor is deprecated, use :obj:`HertzMat` and :obj:`Cp2_HertzMat_HertzPhys` instead.",  \
		/* poisson & en: will move to Cp2_HertzMat_HertzPhys */ \
		/* the following three will disappear */ \
		((Real,gamma,0.0,,"Surface energy parameter [J/m^2] per each unit contact surface, to derive adhesive (DMT, JKR, Schwarz) formulation from HM. If zero, adhesion is disabled..")) \
		((Real,alpha,0.,,"COS alpha parameter")) \
		((bool,warnedDeprec,false,AttrTrait<Attr::noSave|Attr::hidden>(),"Keep track whether we already warned about not using this functor anymore."))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_Cp2_FrictMat_HertzPhys__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(Cp2_FrictMat_HertzPhys);
#endif

struct Cp2_HertzMat_HertzPhys: public Cp2_FrictMat_FrictPhys{
	void go(const shared_ptr<Material>&, const shared_ptr<Material>&, const shared_ptr<Contact>&) override;
	FUNCTOR2D(HertzMat,HertzMat);
	WOO_DECL_LOGGER;
	#define woo_dem_Cp2_HertzMat_HertzPhys__CLASS_BASE_DOC_ATTRS \
		Cp2_HertzMat_HertzPhys,Cp2_FrictMat_FrictPhys,"Compute :obj:`HertzPhys` given two instances of :ref`HertzMat`. The value of :obj:`HertzPhys.alpha` is averaged (from :obj:`HertzMat.alpha`) while :obj:`HertzPhys.gamma` is minimum (of :obj:`HertzMat.gamma`).",  \
		((Real,poisson,.2,,"Poisson ratio for computing contact properties (not provided by the material class currently)")) \
		((Real,en,NaN,,"Normal coefficient of restitution (if outside the 0-1 range, there will be no damping, making ``en`` effectively equal to one)."))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_Cp2_HertzMat_HertzPhys__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(Cp2_HertzMat_HertzPhys);


// Cp2_FrictMat_HertzPhys

struct Law2_L6Geom_HertzPhys_DMT: public LawFunctor{
	bool go(const shared_ptr<CGeom>&, const shared_ptr<CPhys>&, const shared_ptr<Contact>&) override;
	// fast func for computing x^(i/2)
	static Real pow_i_2(const Real& x, const short& i) { return pown(sqrt(x),i);}
	// faster (?) func for computing x^(i/3)
	static Real pow_i_3(const Real& x, const short& i) { return pown(cbrt(x),i);}
	// fast computation of x^(1/4)
	static Real pow_1_4(const Real& x) { return sqrt(sqrt(x)); }
	// normal elastic energy; see Popov2010, pg 60, eq (5.25)
	inline Real normalElasticEnergy(const Real& kn0, const Real& uN){ return kn0*(2/5.)*pow_i_2(uN,5); }
	FUNCTOR2D(L6Geom,HertzPhys);
	WOO_DECL_LOGGER;
	void postLoad(Law2_L6Geom_HertzPhys_DMT&,void*);
	#define WOO_SCHWARZ_COUNTERS
	#ifdef WOO_SCHWARZ_COUNTERS
		Real pyAvgIter(){ avgIter=nCallsIters[1]*1./nCallsIters[0]; return avgIter; }
		void pyResetCounters(){ nCallsIters.resetAll(); }
		#define woo_dem_Law2_L6Geom_HertzPhys_DMT__SCHWARZ_COUNTERS_ATTRS \
			((OpenMPArrayAccumulator<int>,nCallsIters,,AttrTrait<>().noGui().noDump(),"Count number of calls of the functor and of iterations in the Halley solver (if used).")) \
			((Real,avgIter,NaN,AttrTrait<>().buttons({"reset counters","self.resetCounters()",""},/*showBefore*/true).readonly(),"Average number of Halley iterations per contact when using the Schwarz model (updated on-demand)."))
		#define woo_dem_Law2_L6Geom_HertzPhys_DMT__SCHWARZ_COUNTERS_PY \
			.add_property_readonly("avgIter",&Law2_L6Geom_HertzPhys_DMT::pyAvgIter,"Override the variable so that it shows up in the UI, being always up-to-date.") \
			.def("resetCounters",&Law2_L6Geom_HertzPhys_DMT::pyResetCounters,"Reset *nCallsIters* and thus *avgIter*.")
	#else
		#define woo_dem_Law2_L6Geom_HertzPhys_DMT__SCHWARZ_COUNTERS_ATTRS
		#define woo_dem_Law2_L6Geom_HertzPhys_DMT__SCHWARZ_COUNTERS_PY
	#endif
	#define woo_dem_Law2_L6Geom_HertzPhys_DMT__CLASS_BASE_DOC_ATTRS_CTOR_PY \
		Law2_L6Geom_HertzPhys_DMT,LawFunctor,"Law for Hertz contact with optional adhesion (DMT (Derjaguin-Muller-Toporov) :cite:`Derjaguin1975`), non-linear viscosity (:cite:`Antypov2011`) The formulation is taken mainly from :cite:`Johnson1987`. The parameters are given through :obj:`Cp2_FrictMat_HertzPhys`. More details are given in :ref:`hertzian_contact_models`.", \
		((bool,noAttraction,true,,"Avoid non-physical normal attraction which may result from viscous effects by making the normal force zero if there is attraction (:math:`F_n>0`). This condition is only applied to elastic and viscous part of the normal force. Adhesion, if present, is not limited. See :cite:`Antypov2011`, the 'Model choice' section (pg. 5), for discussion of this effect.\n.. note:: It is technically not possible to break the contact completely while there is still geometrical overlap, so only force is set to zero but the contact still exists.")) \
		woo_dem_Law2_L6Geom_HertzPhys_DMT__SCHWARZ_COUNTERS_ATTRS \
		((int,digits,std::numeric_limits<Real>::digits/2,AttrTrait<>().range(Vector2i(1,std::numeric_limits<Real>::digits)),"Precision for Halley iteration with the Schwarz model, measured in binary digits; the maximum is the number of digits of the floating point type for given platform. Precision above 2/3 of the maximum will very likely have no effect on the result, but it will require extra (few) iterations before converging.")) \
		((int,plastIx,-1,AttrTrait<Attr::noSave|Attr::hidden>(),"Index of plastically dissipated energy.")) \
		((int,viscNIx,-1,AttrTrait<Attr::noSave|Attr::hidden>(),"Index of viscous dissipation in the normal sense.")) \
		((int,viscTIx,-1,AttrTrait<Attr::noSave|Attr::hidden>(),"Index of viscous dissipation in the tangent sense.")) \
		((int,elastPotIx,-1,AttrTrait<Attr::noSave|Attr::hidden>(),"Index for elastic potential energy.")) \
		((int,dmtIx,-1,AttrTrait<Attr::noSave|Attr::hidden>(),"Index for elastic energy of new/broken contacts.")) \
		, /* ctor */ nCallsIters.resize(2); \
		, /*py*/ woo_dem_Law2_L6Geom_HertzPhys_DMT__SCHWARZ_COUNTERS_PY
	
	WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR_PY(woo_dem_Law2_L6Geom_HertzPhys_DMT__CLASS_BASE_DOC_ATTRS_CTOR_PY);
};
WOO_REGISTER_OBJECT(Law2_L6Geom_HertzPhys_DMT);

