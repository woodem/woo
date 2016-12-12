#include<woo/pkg/dem/Luding.hpp>
WOO_PLUGIN(dem,(LudingMat)(LudingMatState)(LudingPhys)(Cp2_LudingMat_LudingPhys)(Law2_L6Geom_LudingPhys));
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_LudingMat__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_LudingMatState__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_LudingPhys__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_Cp2_LudingMat_LudingPhys__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_Law2_L6Geom_LudingPhys__CLASS_BASE_DOC_ATTRS);

void Cp2_LudingMat_LudingPhys::go(const shared_ptr<Material>& mat1, const shared_ptr<Material>& mat2, const shared_ptr<Contact>& C){
	#if 0
		if(plastDivKn<0 || plastDivKn>1) throw std::invalid_argument("Cp2_FrictMat_LudingPhys.plstDivKn: must be >0 and <=1 (not "+to_string(plastDivKn));
		if(adhDivKn<0 || adhDivKn>1) throw std::invalid_argument("Cp2_FrictMat_LudingPhys.adhDivKn: must be >0 and <=1 (not "+to_string(adhDivKn));
		if(dynDivStat<0 || dynDivStat>1) throw std::invalid_argument("Cp2_FrictMat_FrictPhys.dynDivStat: must be >0 and <=1 (not "+to_string(dynDivStat));
	#endif

	if(!C->phys) C->phys=make_shared<LudingPhys>();
	auto& m1=mat1->cast<LudingMat>(); auto& m2=mat2->cast<LudingMat>();
	auto& ph=C->phys->cast<LudingPhys>();
	auto& g=C->geom->cast<L6Geom>();
	Cp2_FrictMat_FrictPhys::updateFrictPhys(m1,m2,ph,C);
	
	const Real& a1(g.lens[0]); const Real& a2(g.lens[1]);
	// XXX: meaning of a12=?
	Real a12=(2*a1*a2)/(a1+a2);
	ph.k2hat=ph.kn; 
	const Real& k2hat(ph.k2hat);

	ph.kn1=k2hat*.5*(m1.plastDivKn+m2.plastDivKn);
	ph.knc=k2hat*.5*(m1.adhDivKn+m2.adhDivKn);
	// take the kn value computed in updateFrictPhys, but from now on ph.kn means current stiffness (k2)
	ph.deltaMax=max(-g.uN,0.);
	// eq (7)
	ph.deltaLim=(k2hat/(k2hat-ph.kn1))*.5*(m1.relPlastDepth+m2.relPlastDepth)*a12;
	// a12: dimensional scaling
	ph.kr=k2hat*.5*(m1.krDivKn+m2.krDivKn)*a12;
	ph.kw=k2hat*.5*(m1.kwDivKn+m2.kwDivKn)*a12;
	// minima
	ph.viscN=min(m1.viscN,m2.viscN);
	ph.viscT=min(m1.viscT,m2.viscT);
	ph.viscR=min(m1.viscR,m2.viscR);
	ph.viscW=min(m1.viscW,m2.viscW);
	ph.dynDivStat=min(m1.dynDivStat,m2.dynDivStat);
	ph.statR=min(m1.statR,m2.statR);
	ph.statW=min(m1.statW,m2.statW);
}

void LudingPhys::addWork(int type, const Real& dW){
	assert(type>=0 && type<WORK_SIZE);
	if(work.size()<WORK_SIZE) work.resize(WORK_SIZE,0.);
	work[type]+=dW;
}

void LudingPhys::commitWork(const Scene* scene, const Particle* pA, const Particle* pB, int& viscIx, int& plastIx) const {
	assert(scene->trackEnergy);
	if(work.size()<WORK_SIZE) return;
	scene->energy->add(work[WORK_VISCOUS],"viscous",viscIx,EnergyTracker::IsIncrement | EnergyTracker::ZeroDontCreate);
	scene->energy->add(work[WORK_PLASTIC],"plastic",plastIx,EnergyTracker::IsIncrement | EnergyTracker::ZeroDontCreate);
	for(const Particle* p: {pA,pB}){
		if(!p->matState) continue;
		boost::mutex::scoped_lock l(p->matState->lock);
		assert(p->matState->isA<LudingMatState>());
		auto& lms=p->matState->cast<LudingMatState>();
		// each particle gets only half
		lms.total+=.5*(work[WORK_VISCOUS]+work[WORK_PLASTIC]);
		lms.visco+=.5*(work[WORK_VISCOUS]);
		lms.plast+=.5*(work[WORK_PLASTIC]);
	}
}


// use 3 different types to accomodate combinations of Vector2r, Eigen::Map<Vector2r> etc
template<typename VecT1, typename VecT2, typename VecT3>
void Luding_genericSlidingRoutine(const Real& yieldLim, const Real& k, const Real& visc, const VecT1& vel, VecT2& xi, const Real& dt, const Real& dynDivStat, VecT3& result, bool trackEnergy, LudingPhys& ph){
	static_assert(std::is_same<typename VecT1::Scalar,Real>::value,"Scalar type must be Real");
	// compatible plain dense vector type
	typedef Eigen::Matrix<typename VecT1::Scalar,VecT1::RowsAtCompileTime,1> VecT;
	// trial force
	VecT ft=-k*xi-visc*vel;
	// viscous dissipation
	if(unlikely(trackEnergy)) ph.addWork(LudingPhys::WORK_VISCOUS,visc*vel.squaredNorm()*dt);
	if(ft.squaredNorm()<=yieldLim*yieldLim){
		// eq(19), static friction
		xi+=vel*dt;
		result=ft;
		// no plastic dissipation
	} else {
		// sliding (dynamic) friction norm
		Real fd=yieldLim*dynDivStat;
		Real ftNorm=ft.norm();
		result=(ft/ftNorm)*fd;
		xi=(-1/k)*(result+visc*vel);
		// compute dissipation
		if(unlikely(trackEnergy)) ph.addWork(LudingPhys::WORK_PLASTIC,(1./2)*(ftNorm+fd)*(1./k)*(ftNorm-fd));
	}
}


bool Law2_L6Geom_LudingPhys::go(const shared_ptr<CGeom>& cg, const shared_ptr<CPhys>& cp, const shared_ptr<Contact>& C){
	const L6Geom& g(cg->cast<L6Geom>()); LudingPhys& ph(cp->cast<LudingPhys>());
	// forces and torques in all 4 senses
	Real& Fn(ph.force[0]);  Eigen::Map<Vector2r> Ft(&ph.force[1]);
	Real& Tw(ph.torque[0]); Eigen::Map<Vector2r> Tr(&ph.torque[1]);
	// relative velocities
	const Real& velN(g.vel[0]);
	typedef Eigen::Map<const Vector2r> Map2r;
	typedef Eigen::Map<const Eigen::Matrix<Real,1,1>> Map1r_const;
	typedef Eigen::Map<Eigen::Matrix<Real,1,1>> Map1r;
	Map2r velT(&g.vel[1]);
	Map2r angVelR(&g.angVel[1]);
	//const Real& angVelW(g.angVel[0]);
	//Map1r angVelW_vec1(&g.angVel[1]);

	if(g.uN>0){
		// breaking contact, commit dissipation data now
		if(unlikely(scene->trackEnergy)){
			// compute normal plastic work (non-incremental)
			// use previous kn2 and deltaMax, those won't be increased in the step the contact is broken
			const Real& kn2(ph.kn);
			Real delta0=ph.deltaMax*(ph.kn1-kn2);
			Real deltaMin=delta0*kn2/(kn2+ph.knc);
			ph.addWork(LudingPhys::WORK_PLASTIC,/*W_np*/(ph.kn1*ph.deltaMax+ph.knc*deltaMin)*delta0);
			ph.commitWork(scene,C->leakPA(),C->leakPB(),viscIx,plastIx);
		}
		return false;
	}

	Real delta=-g.uN; // add uN0 here?
	ph.deltaMax=max(delta,ph.deltaMax);
	const Real& k2hat(ph.k2hat);
	const Real& k1(ph.kn1);
	// scaling for forces/torques
	Real a12=(2*g.lens[0]*g.lens[1])/(g.lens[0]+g.lens[1]);
	
	// NORMAL
	// eq (8)
	Real& k2(ph.kn); // current stiffness
	k2=(ph.deltaMax>=ph.deltaLim)?k2hat:(k1+(k2hat-k1)*ph.deltaMax/ph.deltaLim);
	// cerr<<"k2="<<k2<<", a12="<<a12<<endl;
	// in text after eq (6)
	Real delta0=(1-k1/k2)*ph.deltaMax;
	// eq (6)
	Real fHys=NaN;
	Real k2dd0=k2*(delta-delta0);
	if(k2dd0>=k1*delta){
		fHys=k1*delta;
		//ph.normBranch=0;
	} else if(-ph.knc*delta>=k2dd0){
		fHys=-ph.knc*delta;
		//ph.normBranch=2;
	} else{
		assert(k1*delta>k2dd0);
		assert(k2dd0>-ph.knc*delta);
		fHys=k2dd0;
		//ph.normBranch=1;
	}
	assert(!isnan(fHys));
	// after eq (8); final normal force
	Real fn=fHys+ph.viscN*(-1.)*velN; // XXX: velN sign: invert
	// force: viscN*velN; distance: velN*Δt
	if(unlikely(scene->trackEnergy)) ph.addWork(LudingPhys::WORK_VISCOUS,ph.viscN*velN*velN*scene->dt);
	// plastic work computed when contact dissolves

	// TANGENT
	Vector2r ft;
	// coulomb sliding (yield) force
	Real fts=ph.tanPhi*(fn+ph.knc*delta);
	Luding_genericSlidingRoutine(fts,ph.kt,ph.viscT,velT,ph.xiT,scene->dt,ph.dynDivStat,ft,scene->trackEnergy,ph);

	// ROLL
	Vector2r mr;
	Real mrs=ph.statR*(fn+ph.knc*delta)*a12;
	Luding_genericSlidingRoutine(mrs,ph.kr,ph.viscR,angVelR,ph.xiR,scene->dt,ph.dynDivStat,mr,scene->trackEnergy,ph);

	// TWIST
	Real mw;
	Map1r mw_vec(&mw);
	Real mws=ph.statW*(fn+ph.knc*delta)*a12;
	Map1r xiW_vec(&ph.xiW);
	Luding_genericSlidingRoutine(mws,ph.kw,ph.viscW,Map1r_const(&g.angVel[0]),xiW_vec,scene->dt,ph.dynDivStat,mw_vec,scene->trackEnergy,ph);

	// invert signes for applied forces
	Fn=-fn;
	Ft=-ft;
	Tr=-mr;
	Tw=-mw;

	return true;
}




