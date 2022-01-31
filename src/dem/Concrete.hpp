#pragma once

#include<woo/pkg/dem/FrictMat.hpp>
#include<woo/pkg/dem/L6Geom.hpp>

struct ConcreteMatState: public MatState {
	string getScalarName(int index) override {
		switch(index){
			case 0: return "num. broken cohesive";
			default: return "";
		}
	}
	Real getScalar(int index, const Real& time, const long& step, const Real& smooth=0) override {
		switch(index){
			case 0: return numBrokenCohesive;
			default: return NaN;
		}
	}
	size_t getNumScalars() const override { return 1; }
	#define woo_dem_ConcreteMatState__CLASS_BASE_DOC_ATTRS \
		ConcreteMatState,MatState,"State information about body use by the concrete model. None of that is used for computation (at least not now), only for visualization and post-processing.", \
		/*((Real,epsVolumetric,0,,"Volumetric strain around this body (unused for now)"))*/ \
		((int,numBrokenCohesive,0,,"Number of (cohesive) contacts that damaged completely")) \
		/*((int,numContacts,0,,"Number of contacts with this body"))*/ \
		/*((Real,normDmg,0,,"Average damage including already deleted contacts (it is really not damage, but 1-relResidualStrength now)"))*/ \
		/* ((Matrix3r,stress,Matrix3r::Zero(),,"Stress tensor of the spherical particle (under assumption that particle volume = pi*r*r*r*4/3.) for packing fraction 0.62")) */ \
		/* ((Matrix3r,damageTensor,Matrix3r::Zero(),,"Damage tensor computed with microplane theory averaging. state.damageTensor.trace() = state.normDmg")) */
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_ConcreteMatState__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(ConcreteMatState);


struct ConcreteMat: public FrictMat {
	//public:
	//	virtual shared_ptr<State> newAssocState() const { return shared_ptr<State>(new ConcreteMatState); }
	//	virtual bool stateTypeOk(State* s) const { return s->isA<ConcreteMatState>(); }
	enum{ DAMLAW_LINEAR_SOFT=0,DAMLAW_EXPONENTIAL_SOFT=1};
	#define woo_dem_ConcreteMat__CLASS_BASE_DOC_ATTRS_CTOR \
		ConcreteMat,FrictMat,"Concrete material, for use with other Concrete classes.", \
		((Real,sigmaT,NaN,,"Initial cohesion [Pa]")) \
		((bool,neverDamage,false,,"If true, no damage will occur (for testing only).")) \
		((Real,epsCrackOnset,NaN,,"Limit elastic strain [-]")) \
		((Real,relDuctility,NaN,,"relative ductility of bonds in normal direction")) \
		((int,damLaw,1,AttrTrait<Attr::namedEnum>().namedEnum({{DAMLAW_LINEAR_SOFT,{"linear softening","lin"}},{DAMLAW_EXPONENTIAL_SOFT,{"exponential softening","exp"}}}),"Law for damage evolution in uniaxial tension. 0 for linear stress-strain softening branch, 1 (default) for exponential damage evolution law")) \
		((Real,dmgTau,((void)"deactivated if negative",-1),,"Characteristic time for normal viscosity. [s]")) \
		((Real,dmgRateExp,0,,"Exponent for normal viscosity function. [-]")) \
		((Real,plTau,((void)"deactivated if negative",-1),,"Characteristic time for visco-plasticity. [s]")) \
		((Real,plRateExp,0,,"Exponent for visco-plasticity function. [-]")) \
		((Real,isoPrestress,0,,"Isotropic prestress of the whole specimen. [P a]")), \
		/*ctor*/ createIndex(); density=4800;

	WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_ConcreteMat__CLASS_BASE_DOC_ATTRS_CTOR);
	REGISTER_CLASS_INDEX(ConcreteMat,FrictMat);
};
WOO_REGISTER_OBJECT(ConcreteMat);


struct ConcretePhys: public FrictPhys {
	static long cummBetaIter, cummBetaCount;

	static Real solveBeta(const Real c, const Real N);
	Real computeDmgOverstress(Real dt);
	Real computeViscoplScalingFactor(Real sigmaTNorm, Real sigmaTYield,Real dt);

	/* damage evolution law */
	static Real funcG(const Real& kappaD, const Real& epsCrackOnset, const Real& epsFracture, const bool& neverDamage, const int& damLaw);
	static Real funcGDKappa(const Real& kappaD, const Real& epsCrackOnset, const Real& epsFracture, const bool& neverDamage, const int& damLaw);
	/* inverse damage evolution law */
	static Real funcGInv(const Real& omega, const Real& epsCrackOnset, const Real& epsFracture, const bool& neverDamage, const int& damLaw);
	void setDamage(Real dmg);
	void setRelResidualStrength(Real r);

	#define woo_dem_ConcretePhys__CLASS_BASE_DOC_ATTRS_CTOR_PY \
		ConcretePhys,FrictPhys,"Representation of a single interaction of the Concrete type: storage for relevant parameters.", \
		((Real,E,NaN,,"normal modulus (stiffness / crossSection) [Pa]")) \
		((Real,G,NaN,,"shear modulus [Pa]")) \
		((Real,coh0,NaN,,"virgin material cohesion [Pa]")) \
		((Real,epsCrackOnset,NaN,,"strain at which the material starts to behave non-linearly")) \
		/*((Real,relDuctility,NaN,,"Relative ductility of bonds in normal direction"))*/ \
		((Real,epsFracture,NaN,,"strain at which the bond is fully broken [-]")) \
		((Real,dmgTau,-1,,"characteristic time for damage (if non-positive, the law without rate-dependence is used)")) \
		((Real,dmgRateExp,0,,"exponent in the rate-dependent damage evolution")) \
		((Real,dmgStrain,0,,"damage strain (at previous or current step)")) \
		((Real,dmgOverstress,0,,"damage viscous overstress (at previous step or at current step)")) \
		((Real,plTau,-1,,"characteristic time for viscoplasticity (if non-positive, no rate-dependence for shear)")) \
		((Real,plRateExp,0,,"exponent in the rate-dependent viscoplasticity")) \
		((Real,isoPrestress,0,,"\"prestress\" of this link (used to simulate isotropic stress)")) \
		((bool,neverDamage,false,,"the damage evolution function will always return virgin state")) \
		((int,damLaw,1,,"Law for softening part of uniaxial tension. 0 for linear, 1 for exponential (default)")) \
		((bool,isCohesive,false,,"if not cohesive, interaction is deleted when distance is greater than zero.")) \
		((Vector2r,epsT,Vector2r::Zero(),AttrTrait<Attr::noSave|Attr::readonly>(),"Shear strain; updated incrementally from :obj:`L6Geom.vel`.")) \
		\
		((Real,omega,0,AttrTrait<Attr::noSave|Attr::readonly>().startGroup("Auto-computed"),"Damage parameter")) \
		((Real,uN0,NaN,AttrTrait<Attr::readonly>(),"Initial normal displacement (equilibrium state)")) \
		((Real,epsN,0,AttrTrait<Attr::noSave|Attr::readonly>(),"Normal strain")) \
		((Real,sigmaN,0,AttrTrait<Attr::noSave|Attr::readonly>(),"Normal force")) \
		((Vector2r,sigmaT,Vector2r::Zero(),AttrTrait<Attr::noSave|Attr::readonly>(),"Tangential stress")) \
		((Real,epsNPl,0,AttrTrait<Attr::noSave|Attr::readonly>(),"Normal plastic strain")) \
		/*((Vector2r,epsTPl,Vector3r::Zero(),AttrTrait<Attr::noSave|Attr::readonly>(),"Shear plastic strain"))*/ \
		((Real,kappaD,0,AttrTrait<Attr::noSave|Attr::readonly>(),"Value of the kappa function")) \
		((Real,relResidualStrength,1,AttrTrait<Attr::noSave|Attr::readonly>(),"Relative residual strength")) \
		,/*ctor*/ createIndex(); \
		,/*py*/  \
		.def_readonly_static("cummBetaIter",&ConcretePhys::cummBetaIter,"Cummulative number of iterations inside ConcreteMat::solveBeta (for debugging).") \
		.def_readonly_static("cummBetaCount",&ConcretePhys::cummBetaCount,"Cummulative number of calls of ConcreteMat::solveBeta (for debugging).") \
		.def_static("funcG",&ConcretePhys::funcG,WOO_PY_ARGS(py::arg("kappaD"),py::arg("epsCrackOnset"),py::arg("epsFracture"),py::arg("neverDamage")=false,py::arg("damLaw")=1),"Damage evolution law, evaluating the $\\omega$ parameter. $\\kappa_D$ is historically maximum strain, *epsCrackOnset* ($\\varepsilon_0$) = :obj:`epsCrackOnset`, *epsFracture* = :obj:`epsFracture`; if *neverDamage* is ``True``, the value returned will always be 0 (no damage).") \
		 \
		.def_static("funcGInv",&ConcretePhys::funcGInv,WOO_PY_ARGS(py::arg("omega"),py::arg("epsCrackOnset"),py::arg("epsFracture"),py::arg("neverDamage")=false,py::arg("damLaw")=1),"Inversion of damage evolution law, evaluating the $\\kappa_D$ parameter. $\\omega$ is damage, for other parameters see funcG function") \
		 \
		.def("setDamage",&ConcretePhys::setDamage,"TODO") \
		.def("setRelResidualStrength",&ConcretePhys::setRelResidualStrength,"TODO")
	
	WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR_PY(woo_dem_ConcretePhys__CLASS_BASE_DOC_ATTRS_CTOR_PY);
	WOO_DECL_LOGGER;
	REGISTER_CLASS_INDEX(ConcretePhys,FrictPhys);
};
WOO_REGISTER_OBJECT(ConcretePhys);

struct Cp2_ConcreteMat_ConcretePhys: public CPhysFunctor{
	void go(const shared_ptr<Material>&, const shared_ptr<Material>&, const shared_ptr<Contact>&) override;
	FUNCTOR2D(ConcreteMat,ConcreteMat);
	WOO_DECL_LOGGER;
	#define woo_dem_Cp2_ConcreteMat_ConcretePhys__CLASS_BASE_DOC_ATTRS \
		Cp2_ConcreteMat_ConcretePhys,CPhysFunctor,"Compute :obj:`ConcretePhys` from two :obj:`ConcreteMat` instances. Uses simple (arithmetic) averages if material are different. Simple copy of parameters is performed if the instance of :obj:`ConcreteMat` is shared.", \
		((long,cohesiveThresholdStep,10,,"Should new contacts be cohesive? They will before this iter#, they will not be afterwards. If 0, they will never be. If negative, they will always be created as cohesive (10 by default)."))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_Cp2_ConcreteMat_ConcretePhys__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(Cp2_ConcreteMat_ConcretePhys);


struct Law2_L6Geom_ConcretePhys: public LawFunctor{
	// Real elasticEnergy();
	enum{YIELD_MOHRCOULOMB=0,YIELD_PARABOLIC,YIELD_LOG,YIELD_LOG_LIN,YIELD_ELLIPTIC,YIELD_ELLIPTIC_LOG};
	Real yieldSigmaTNorm(Real sigmaN, Real omega, Real coh0, Real tanPhi) {
		Real ret=NaN;
		switch(yieldSurfType){
			case YIELD_MOHRCOULOMB: ret=coh0*(1-omega)-sigmaN*tanPhi; break;
			case YIELD_PARABOLIC: ret=sqrt(pow2(coh0)-2*coh0*tanPhi*sigmaN)-coh0*omega; break;
			case YIELD_LOG:
				ret=( (sigmaN/(coh0*yieldLogSpeed))>=1 ? 0 : coh0*((1-omega)+(tanPhi*yieldLogSpeed)*log(-sigmaN/(coh0*yieldLogSpeed)+1)));
				break;
			case YIELD_LOG_LIN:
				if(sigmaN>0){ ret=coh0*(1-omega)-sigmaN*tanPhi; break; }
				ret=( (sigmaN/(coh0*yieldLogSpeed))>=1 ? 0 : coh0*((1-omega)+(tanPhi*yieldLogSpeed)*log(-sigmaN/(coh0*yieldLogSpeed)+1)));
				break;
			case YIELD_ELLIPTIC:
			case YIELD_ELLIPTIC_LOG:
				if(yieldSurfType==YIELD_ELLIPTIC && sigmaN>0){ ret=coh0*(1-omega)-sigmaN*tanPhi; break; }
				ret=(pow2(sigmaN-yieldEllipseShift)/(-yieldEllipseShift*coh0/((1-omega)*tanPhi)+pow2(yieldEllipseShift)))>=1 ? 0 : sqrt((-coh0*((1-omega)*tanPhi)*yieldEllipseShift+pow2(coh0))*(1-pow2(sigmaN-yieldEllipseShift)/(-yieldEllipseShift*coh0/((1-omega)*tanPhi)+pow2(yieldEllipseShift))))-omega*coh0; break;
			default: throw std::logic_error("Law2_L6Geom_ConcretePhys::yieldSigmaTNorm: invalid value of yieldSurfType="+to_string(yieldSurfType));
		}
		assert(!isnan(ret));
		return max(0.,ret);
	}

	bool go(const shared_ptr<CGeom>&, const shared_ptr<CPhys>&, const shared_ptr<Contact>&) override;
	FUNCTOR2D(L6Geom,ConcretePhys);
	WOO_DECL_LOGGER;
	#define woo_dem_Law2_L6Geom_ConcretePhys__CLASS_BASE_DOC_ATTRS_PY \
		Law2_L6Geom_ConcretePhys,LawFunctor,"Constitutive law for concrete.", \
		((int,yieldSurfType,YIELD_LOG_LIN,AttrTrait<Attr::namedEnum>().namedEnum({{YIELD_MOHRCOULOMB,{"linear","lin","MC","mc","Mohr-Coulomb"}},{YIELD_PARABOLIC,{"para","parabolic"}},{YIELD_LOG,{"log","logarithmic"}},{YIELD_LOG_LIN,{"log+lin","logarithmic, linear tension","loglin"}},{YIELD_ELLIPTIC,{"elliptic","ell"}},{YIELD_ELLIPTIC_LOG,{"elliptic+logarithmic","ell+log"}}}),"yield function: 0: mohr-coulomb (original); 1: parabolic; 2: logarithmic, 3: log+lin_tension, 4: elliptic, 5: elliptic+log")) \
		((Real,yieldLogSpeed,.1,,"scaling in the logarithmic yield surface (should be <1 for realistic results; >=0 for meaningful results)")) \
		((Real,yieldEllipseShift,NaN,,"horizontal scaling of the ellipse (shifts on the +x axis as interactions with +y are given)")) \
		((Real,omegaThreshold,((void)">=1. to deactivate, i.e. never delete any contacts",1.),,"damage after which the contact disappears (<1), since omega reaches 1 only for strain →+∞")) \
		((Real,epsSoft,((void)"approximates confinement -20MPa precisely, -100MPa a little over, -200 and -400 are OK (secant)",-3e-3),,"Strain at which softening in compression starts (non-negative to deactivate)")) \
		((Real,relKnSoft,.3,,"Relative rigidity of the softening branch in compression (0=perfect elastic-plastic, <0 softening, >0 hardening)")) \
		((int,elastPotIx,-1,AttrTrait<Attr::noSave|Attr::hidden>(),"Index for elastic potential energy")) \
		, /*py*/.def("yieldSigmaTNorm",&Law2_L6Geom_ConcretePhys::yieldSigmaTNorm,WOO_PY_ARGS(py::arg("sigmaN"),py::arg("omega"),py::arg("coh0"),py::arg("tanPhi")),"Return radius of yield surface for given material and state parameters; uses attributes of the current instance (:obj:`yieldSurfType` etc), change them before calling if you need that.") \
			
	WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_Law2_L6Geom_ConcretePhys__CLASS_BASE_DOC_ATTRS_PY);
};
WOO_REGISTER_OBJECT(Law2_L6Geom_ConcretePhys);



#ifdef WOO_OPENGL

#include<woo/pkg/dem/Gl1_CPhys.hpp>

class GLUquadric;

class Gl1_ConcretePhys: public Gl1_CPhys{	
	static GLUquadric* gluQuadric; // needed for gluCylinder, initialized by ::go if no initialized yet
	public:
		void go(const shared_ptr<CPhys>&,const shared_ptr<Contact>&, const GLViewInfo&) override;
	enum{CPHYS_NO,CPHYS_ONLY,CPHYS_ALSO};
	#define woo_dem_Gl1_ConcretePhys__CLASS_BASE_DOC_ATTRS \
		Gl1_ConcretePhys,Gl1_CPhys,"Renders :obj:`ConcretePhys` objects with discs representing damage. :obj:`Gl1_CPhys` may be requested to do other rendering based on :obj:`doCPhys`.", \
		((int,doCPhys,CPHYS_NO,AttrTrait<Attr::namedEnum>().namedEnum({{CPHYS_NO,{"no",""}},{CPHYS_ONLY,{"only"}},{CPHYS_ALSO,{"also"}}}),"Call :obj:`Gl1_CPhys` for rendering.")) \
		((shared_ptr<ScalarRange>,dmgRange,make_shared<ScalarRange>(),,"Range for disk damage coloring.")) \
		((int,dmgSlices,6,,"Number of slices to draw the damage disc")) \
		((int,dmgPow,2,,"Raise :obj:`~ConcretePhys.omega` to this power for disk radius scaling; 2 makes the disc area (rather than radius) proportional to omega."))

	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_Gl1_ConcretePhys__CLASS_BASE_DOC_ATTRS);
	RENDERS(ConcretePhys);
};
WOO_REGISTER_OBJECT(Gl1_ConcretePhys);
#endif

