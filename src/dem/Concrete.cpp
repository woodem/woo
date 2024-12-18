#include"Concrete.hpp"

WOO_PLUGIN(dem,(ConcreteMatState)(ConcreteMat)(ConcretePhys)(Cp2_ConcreteMat_ConcretePhys)(Law2_L6Geom_ConcretePhys));
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_ConcreteMatState__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_ConcreteMat__CLASS_BASE_DOC_ATTRS_CTOR);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR_PY(woo_dem_ConcretePhys__CLASS_BASE_DOC_ATTRS_CTOR_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_Cp2_ConcreteMat_ConcretePhys__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_Law2_L6Geom_ConcretePhys__CLASS_BASE_DOC_ATTRS_PY);
	


WOO_IMPL_LOGGER(Cp2_ConcreteMat_ConcretePhys);

void Cp2_ConcreteMat_ConcretePhys::go(const shared_ptr<Material>& m1, const shared_ptr<Material>& m2, const shared_ptr<Contact>& C){
	// no updates of an already existing contact necessary
	if(C->phys) return;
	auto p=make_shared<ConcretePhys>();
	C->phys=p;
	const auto& mat1=m1->cast<ConcreteMat>();
	const auto& mat2=m2->cast<ConcreteMat>();

	// check unassigned values
	if (!mat1.neverDamage) {
		assert(!isnan(mat1.sigmaT));
		assert(!isnan(mat1.epsCrackOnset));
		assert(!isnan(mat1.relDuctility));
	}
	if (!mat2.neverDamage) {
		assert(!isnan(mat2.sigmaT));
		assert(!isnan(mat2.epsCrackOnset));
		assert(!isnan(mat2.relDuctility));
	}

	// particles sharing the same material; no averages necessary
	if (m1.get()==m2.get()) {
		p->E=mat1.young;
		p->G=mat1.young*mat1.ktDivKn;
		p->tanPhi=mat1.tanPhi;
		p->coh0=mat1.sigmaT;
		p->isCohesive=(cohesiveThresholdStep<0 || scene->step<cohesiveThresholdStep);
		#define _CPATTR(a) p->a=mat1.a
			_CPATTR(epsCrackOnset);
			_CPATTR(neverDamage);
			_CPATTR(dmgTau);
			_CPATTR(dmgRateExp);
			_CPATTR(plTau);
			_CPATTR(plRateExp);
			_CPATTR(isoPrestress);
		#undef _CPATTR
	} else {
		// averaging over both materials
		#define _AVGATTR(a) p->a=.5*(mat1.a+mat2.a)
			p->E=.5*(mat1.young+mat2.young);
			p->G=.5*(mat1.ktDivKn+mat2.ktDivKn)*.5*(mat1.young+mat2.young);
			_AVGATTR(tanPhi);
			p->coh0=.5*(mat1.sigmaT+mat2.sigmaT);
			p->isCohesive=(cohesiveThresholdStep<0 || scene->step<cohesiveThresholdStep);
			_AVGATTR(epsCrackOnset);
			p->neverDamage=(mat1.neverDamage || mat2.neverDamage);
			_AVGATTR(dmgTau);
			_AVGATTR(dmgRateExp);
			_AVGATTR(plTau);
			_AVGATTR(plRateExp);
			_AVGATTR(isoPrestress);
		#undef _AVGATTR
	}

	if(mat1.damLaw!=mat2.damLaw) throw std::runtime_error("Cp2_ConcreteMat_ConcretePhys: damLaw is not the same for "+C->leakPA()->pyStr()+" and "+C->leakPB()->pyStr()+".");
	p->damLaw=mat1.damLaw;
	p->epsFracture=p->epsCrackOnset*.5*(mat1.relDuctility+mat2.relDuctility);

	// geometry-dependent
	assert(C->geom->isA<L6Geom>());
	const auto& g(C->geom->cast<L6Geom>());
	p->kn=p->E*g.contA/g.lens.sum();
	p->kt=p->G*g.contA/g.lens.sum();
}


WOO_IMPL_LOGGER(ConcretePhys);


long ConcretePhys::cummBetaIter=0, ConcretePhys::cummBetaCount=0;

Real ConcretePhys::solveBeta(const Real c, const Real N){
	#ifdef WOO_DEBUG
		cummBetaCount++;
	#endif
	const int maxIter=20;
	const Real maxError=1e-12;
	Real f, ret=0.;
	for(int i=0; i<maxIter; i++){
		#ifdef WOO_DEBUG
			cummBetaIter++;
		#endif
		Real aux=c*exp(N*ret)+exp(ret);
		f=log(aux);
		if(std::abs(f)<maxError) return ret;
		Real df=(c*N*exp(N*ret)+exp(ret))/aux;
		ret-=f/df;
	}
	LOG_FATAL("No convergence after {} iters; c={}, N={}, ret={}, f={}",maxIter,c,N,ret,f);
	throw runtime_error("ConcretePhys::solveBeta failed to converge.");
}


Real ConcretePhys::computeDmgOverstress(Real dt){
	if(dmgStrain>=epsN*omega) { // unloading, no viscous stress
		dmgStrain=epsN*omega;
		LOG_TRACE("Elastic/unloading, no viscous overstress");
		return 0.;
	}
	Real c=epsCrackOnset*(1-omega)*pow(dmgTau/dt,dmgRateExp)*pow(epsN*omega-dmgStrain,dmgRateExp-1.);
	Real beta=solveBeta(c,dmgRateExp);
	Real deltaDmgStrain=(epsN*omega-dmgStrain)*exp(beta);
	dmgStrain+=deltaDmgStrain;
	LOG_TRACE("deltaDmgStrain={}, viscous overstress {}",deltaDmgStrain,(epsN*omega-dmgStrain)*E);
	/* σN=Kn(εN-εd); dmgOverstress=σN-(1-ω)*Kn*εN=…=Kn(ω*εN-εd) */
	return (epsN*omega-dmgStrain)*E;
}

Real ConcretePhys::computeViscoplScalingFactor(Real sigmaTNorm, Real sigmaTYield,Real dt){
	if(sigmaTNorm<sigmaTYield) return 1.;
	Real c=coh0*pow(plTau/(G*dt),plRateExp)*pow(sigmaTNorm-sigmaTYield,plRateExp-1.);
	Real beta=solveBeta(c,plRateExp);
	//LOG_DEBUG("scaling factor {}",1.-exp(beta)*(1-sigmaTYield/sigmaTNorm));
	return 1.-exp(beta)*(1-sigmaTYield/sigmaTNorm);
}

Real ConcretePhys::funcG(const Real& kappaD, const Real& epsCrackOnset, const Real& epsFracture, const bool& neverDamage, const int& damLaw) {
	if (kappaD<epsCrackOnset || neverDamage) return 0;
	switch(damLaw){
		case ConcreteMat::DAMLAW_LINEAR_SOFT:
			return (1.-epsCrackOnset/kappaD)/(1.-epsCrackOnset/epsFracture);
		case ConcreteMat::DAMLAW_EXPONENTIAL_SOFT:
			return 1.-(epsCrackOnset/kappaD)*exp(-(kappaD-epsCrackOnset)/epsFracture);
	}
	throw runtime_error("ConcretePhys::funcG: wrong damLaw "+to_string(damLaw)+".");
}

Real ConcretePhys::funcGDKappa(const Real& kappaD, const Real& epsCrackOnset, const Real& epsFracture, const bool& neverDamage, const int& damLaw) {
	switch(damLaw){
		case ConcreteMat::DAMLAW_LINEAR_SOFT:
			return epsCrackOnset/((1.-epsCrackOnset/epsFracture)*kappaD*kappaD);
		case ConcreteMat::DAMLAW_EXPONENTIAL_SOFT:
			return epsCrackOnset/kappaD*(1./kappaD+1./epsFracture)*exp(-(kappaD-epsCrackOnset)/epsFracture);
	}
	throw runtime_error("ConcretePhys::funcGDKappa: wrong damLaw "+to_string(damLaw)+".");
}

Real ConcretePhys::funcGInv(const Real& omega, const Real& epsCrackOnset, const Real& epsFracture, const bool& neverDamage, const int& damLaw) {
	if(omega==0. || neverDamage) return 0;
	switch(damLaw){
		case ConcreteMat::DAMLAW_LINEAR_SOFT:
			return epsCrackOnset/(1.-omega*(1.-epsCrackOnset/epsFracture));
		case ConcreteMat::DAMLAW_EXPONENTIAL_SOFT:
			// Newton's iterations
			Real fg, dfg, decr, ret=epsCrackOnset,tol=1e-3;
			int maxIter = 100;
			for (int i=0; i<maxIter; i++) {
				fg=-omega+1.-epsCrackOnset/ret*exp(-(ret-epsCrackOnset)/epsFracture);
				//dfg=(epsCrackOnset/ret/ret-epsCrackOnset*(ret-epsCrackOnset)/ret/epsFracture/epsFracture)*exp(-(ret-epsCrackOnset)/epsFracture);
				dfg=ConcretePhys::funcGDKappa(ret,epsCrackOnset,epsFracture,neverDamage,damLaw);
				decr=fg/dfg;
				ret-=decr;
				if (std::abs(decr/epsCrackOnset)<tol) {
					return ret;
				}
			}
			throw runtime_error("ConcretePhys::funcGInv: no convergence");
	}
	throw runtime_error("ConcretePhys::funcGInv: wrong damLaw "+to_string(damLaw)+".");
}

void ConcretePhys::setDamage(Real dmg) {
	if(neverDamage){ return; }
	omega=dmg;
	kappaD=ConcretePhys::funcGInv(dmg,epsCrackOnset,epsFracture,neverDamage,damLaw);
}

void ConcretePhys::setRelResidualStrength(Real r) {
	if(neverDamage){ return; }
	if(r==1.){
		relResidualStrength=r;
		kappaD=omega=0.;
		return;
	}
	Real k=epsFracture;
	Real g, dg, f, df, tol=1e-3, e0i=1./epsCrackOnset, decr;
	int maxIter=100;
	int i;
	for(i=0; i<maxIter; i++){
		g=ConcretePhys::funcG(k,epsCrackOnset,epsFracture,neverDamage,damLaw);
		dg=ConcretePhys::funcGDKappa(k,epsCrackOnset,epsFracture,neverDamage,damLaw);
		f=-r+(1-g)*k*e0i;
		df=e0i*(1-g-k*dg);
		decr=f/df;
		k-=decr;
		if(std::abs(decr)<tol){
			kappaD=k;
			omega=ConcretePhys::funcG(k,epsCrackOnset,epsFracture,neverDamage,damLaw);
			relResidualStrength=r;
			return;
		}
	}
	throw runtime_error("ConcretePhys::setRelResidualStrength: no convergence.");
}



#define _WOO_VERIFY(condition) if(!(condition)){ throw std::runtime_error(__FILE__+string(":")+to_string(__LINE__)+": verification "+#condition+" failed, in contact "+C->pyStr()+"."); }
#define NNAN(a) _WOO_VERIFY(!isnan(a));
#define NNANV(v) _WOO_VERIFY(!isnan(v.maxCoeff()));

WOO_IMPL_LOGGER(Law2_L6Geom_ConcretePhys);


bool Law2_L6Geom_ConcretePhys::go(const shared_ptr<CGeom>& _geom, const shared_ptr<CPhys>& _phys, const shared_ptr<Contact>& C){
	L6Geom& geom=_geom->cast<L6Geom>();
	ConcretePhys& phys=_phys->cast<ConcretePhys>();

	#if 0
		/* just the first time */
		// TODO: replace by L6Geom.contA, lens and friends
		if (C->isFresh(scene)) {
			const shared_ptr<Body> b1 = Body::byId(I->id1,scene);
			const shared_ptr<Body> b2 = Body::byId(I->id2,scene);
			const int sphereIndex = Sphere::getClassIndexStatic();
			const int facetIndex = Facet::getClassIndexStatic();
			const int wallIndex = Wall::getClassIndexStatic();
			const int b1index = b1->shape->getClassIndex();
			const int b2index = b2->shape->getClassIndex();
			if (b1index == sphereIndex && b2index == sphereIndex) { // both bodies are spheres
				const Vector3r& pos1 = Body::byId(I->id1,scene)->state->pos;
				const Vector3r& pos2 = Body::byId(I->id2,scene)->state->pos;
				Real minRad = (geom->refR1 <= 0? geom->refR2 : (geom->refR2 <=0? geom->refR1 : min(geom->refR1,geom->refR2)));
				Vector3r shift2 = scene->isPeriodic? Vector3r(scene->cell->hSize*I->cellDist.cast<Real>()) : Vector3r::Zero();
				phys->refLength = (pos2 - pos1 + shift2).norm();
				phys->crossSection = Mathr::PI*pow2(minRad);
				phys->refPD = geom->refR1 + geom->refR2 - phys->refLength;
			} else if (b1index == facetIndex || b2index == facetIndex || b1index == wallIndex || b2index == wallIndex) { // one body is facet or wall
				shared_ptr<Body> sphere, plane;
				if (b1index == facetIndex || b1index == wallIndex) { plane = b1; sphere = b2; }
				else { plane = b2; sphere = b1; }
				Real rad = ( (Sphere*) sphere->shape.get() )->radius;
				phys->refLength = rad;
				phys->crossSection = Mathr::PI*pow2(rad);
				phys->refPD = 0.;
			}

			phys->kn = phys->crossSection*phys->E/phys->refLength;
			phys->ks = phys->crossSection*phys->G/phys->refLength;
			phys->epsFracture = phys->epsCrackOnset*phys->relDuctility;
		}
	#endif

	static bool _w=false;
	if(!_w && (yieldSurfType==YIELD_ELLIPTIC || yieldSurfType==YIELD_ELLIPTIC_LOG)){
		_w=true;
		LOG_ERROR("Elliptic yield surface equations in the concrete model are incorrect (and never were). Expect bogus results.");
	}


	
	if(C->isFresh(scene)) phys.uN0=geom.uN;

	phys.epsN=(geom.uN-phys.uN0)/geom.lens.sum();

	phys.epsT+=scene->dt*geom.vel.tail<2>()/geom.lens.sum();

	Real& epsN(phys.epsN);
	Vector2r& epsT(phys.epsT);
	Real& sigmaN(phys.sigmaN);
	Vector2r& sigmaT(phys.sigmaT);
	Real& Fn(phys.force[0]); Eigen::Map<Vector2r> Ft(&phys.force[1]);
	Real& kappaD(phys.kappaD);

	/* Real& epsPlSum(phys->epsPlSum); */
	const Real& E(phys.E);
	const Real& coh0(phys.coh0);
	const Real& tanPhi(phys.tanPhi);
	const Real& G(phys.G);
	const Real& contA(geom.contA);
	const Real& epsCrackOnset(phys.epsCrackOnset);
	Real& relResidualStrength(phys.relResidualStrength);
	const Real& epsFracture(phys.epsFracture);
	const int& damLaw(phys.damLaw);
	const bool& neverDamage(phys.neverDamage);
	Real& omega(phys.omega);
	const bool& isCohesive(phys.isCohesive);

	// Vector3r& epsTPl(phys->epsTPl); // unused
	Real& epsNPl(phys.epsNPl);
	const Real& dt(scene->dt);
	const Real& dmgTau(phys.dmgTau);
	const Real& plTau(phys.plTau);

	NNAN(epsN); NNANV(epsT);

	/* constitutive law */
	Real epsNorm=max(epsN-epsNPl,0.); /* sqrt(epsN**2); */
	kappaD=max(epsNorm,kappaD); /* kappaD is non-decreasing */
	omega=isCohesive?ConcretePhys::funcG(kappaD,epsCrackOnset,epsFracture,neverDamage,damLaw):1.; /* damage parameter; simulate non-cohesive contact via full damage */
	sigmaN=(1-(epsN-epsNPl>0?omega:0))*E*(epsN-epsNPl); /* normal stress */
	if((epsSoft<0) && (epsN-epsNPl<epsSoft)){ /* plastic slip in compression */
		Real sigmaNSoft=E*(epsSoft+relKnSoft*(epsN-epsSoft));
		if(sigmaNSoft>sigmaN){ /*assert(sigmaNSoft>sigmaN);*/ epsNPl+=(sigmaN-sigmaNSoft)/E; sigmaN=sigmaNSoft; }
	}
	relResidualStrength=isCohesive?(kappaD<epsCrackOnset?1.:(1-omega)*(kappaD)/epsCrackOnset):0;

	/* strain return: Coulomb friction */
	sigmaT=G*epsT; // trial stress
	Real rT=yieldSigmaTNorm(sigmaN,omega,coh0,tanPhi);  // radius of plastic zone in tangent direction
	if(sigmaT.squaredNorm()>rT*rT){
		Real sigmaTNorm=sigmaT.norm();
		Real scale=plTau>0 ? phys.computeViscoplScalingFactor(sigmaTNorm,rT,dt) : rT/sigmaTNorm;
		sigmaT*=scale; /* radial shear stress return */
		/* plastic strain multiplied by stress: dissipated energy */
		/* epsPlSum+=rT*contGeom->slipToStrainTMax((plTau>0 ? sigmaT.norm() : rT)/G); */
	}

	/* viscous normal overstress */
	if(dmgTau>0. && isCohesive){ sigmaN+=phys.computeDmgOverstress(dt); } 

	sigmaN-=phys.isoPrestress;
   
   NNAN(sigmaN); NNANV(sigmaT); NNAN(contA);
   if (!neverDamage) { NNAN(kappaD); NNAN(epsFracture); NNAN(omega); }

	/* broken contact */
	if (epsN>0. && ((isCohesive && omega>omegaThreshold) || !isCohesive)) {
		for(const Particle* p: {C->leakPA(), C->leakPB()}){
			if(!p->matState) continue;
			assert(p->matState->isA<ConcreteMatState>());
			auto& cms=p->matState->cast<ConcreteMatState>();
			std::scoped_lock l(cms.lock);
			cms.numBrokenCohesive+=1;
		}
		return false;
	}

	Fn=sigmaN*contA;
	Ft=sigmaT*contA;

	if(WOO_UNLIKELY(scene->trackEnergy)){
		Real knDmg=(1-(epsN-epsNPl>0?omega:0))*phys.kn;
		scene->energy->add(0.5*((knDmg>0?pow2(Fn)/knDmg:0.)+Ft.squaredNorm()/phys.kt),"elast",elastPotIx,EnergyTracker::IsResettable);
	}

	// TIMING_DELTAS_CHECKPOINT("GO B");
	return true;
}



#undef _WOO_VERIFY
#undef _NNAN
#undef _NNANV



#ifdef WOO_OPENGL

#include"../gl/Renderer.hpp" // for GlData
#include<GL/glu.h>

WOO_PLUGIN(gl,(Gl1_ConcretePhys));
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_Gl1_ConcretePhys__CLASS_BASE_DOC_ATTRS);

void Gl1_ConcretePhys::go(const shared_ptr<CPhys>& cp, const shared_ptr<Contact>& C, const GLViewInfo& viewInfo){
	if(doCPhys==CPHYS_ALSO || doCPhys==CPHYS_ONLY) Gl1_CPhys::go(cp,C,viewInfo);
	//cerr<<"A"<<endl;
	if(doCPhys==CPHYS_ONLY) return;
	//cerr<<"B"<<endl;
	if(!dmgRange) return;
	const auto& ph=cp->cast<ConcretePhys>();
	//cerr<<"C"<<endl;
	if(ph.omega==0) return; // no damage, nothing to plot
	static GLUquadric* gluQuadric;
	if(!gluQuadric) gluQuadric=gluNewQuadric(); assert(gluQuadric);
	Vector3r color=dmgRange->color(ph.omega);
	//cerr<<"D"<<endl;
	if(isnan(color.maxCoeff())) return;
	//cerr<<"E"<<endl;
	const auto& ge=C->geom->cast<L6Geom>();
	glPushMatrix();
		glTranslatev((ge.node->pos+ge.node->getData<GlData>().dGlPos).eval());
		// rotate local x to local z, then rotate to global coords (read from right)
		Quaternionr q(ge.node->ori*Quaternionr(AngleAxisr(-M_PI/2.,Vector3r::UnitY()))); 
		glMultMatrixd(Eigen::Affine3d(q).data());
		Real rMax=sqrt(ge.contA/M_PI);
		glColor3v(color);
		gluDisk(gluQuadric,/*inner*/0,/*outer*/rMax*dmgRange->norm(pow(ph.omega,dmgPow)),/*slices*/dmgSlices,/*loops*/1);
	glPopMatrix();
}

#endif
