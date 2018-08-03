#include<woo/pkg/dem/Luding.hpp>
WOO_PLUGIN(dem,(LudingMat)(LudingMatState)(LudingPhys)(Cp2_LudingMat_LudingPhys)(Law2_L6Geom_LudingPhys));
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_LudingMat__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_LudingMatState__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_LudingPhys__CLASS_BASE_DOC_ATTRS_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_Cp2_LudingMat_LudingPhys__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_Law2_L6Geom_LudingPhys__CLASS_BASE_DOC_ATTRS);

void Cp2_LudingMat_LudingPhys::go(const shared_ptr<Material>& mat1, const shared_ptr<Material>& mat2, const shared_ptr<Contact>& C){
	#if 0
		if(k1DivKn<0 || k1DivKn>1) throw std::invalid_argument("Cp2_FrictMat_LudingPhys.plstDivKn: must be >0 and <=1 (not "+to_string(k1DivKn));
		if(kaDivKn<0 || kaDivKn>1) throw std::invalid_argument("Cp2_FrictMat_LudingPhys.kaDivKn: must be >0 and <=1 (not "+to_string(kaDivKn));
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

	ph.kn1=k2hat*.5*(m1.k1DivKn+m2.k1DivKn);
	ph.kna=k2hat*.5*(m1.kaDivKn+m2.kaDivKn);
	// take the kn value computed in updateFrictPhys, but from now on ph.kn means current stiffness (k2)
	ph.deltaMax=max(-g.uN,0.);
	// eq (7)
	ph.deltaLim=(k2hat/(k2hat-ph.kn1))*.5*(m1.deltaLimRel+m2.deltaLimRel)*a12;
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


void Law2_L6Geom_LudingPhys::addWork(LudingPhys& ph, int type, const Real& dW, const Vector3r& xyz) {
	assert(type>=0 && type<LudingPhys::WORK_SIZE);
	if(ph.work.size()<LudingPhys::WORK_SIZE) ph.work.resize(LudingPhys::WORK_SIZE,0.);
	ph.work[type]+=dW;
	if(wImmediate){
		switch(type){
			case LudingPhys::WORK_VISCOUS: scene->energy->add(dW,"viscous",viscIx,EnergyTracker::IsIncrement | EnergyTracker::ZeroDontCreate,xyz); break;
			case LudingPhys::WORK_PLASTIC: scene->energy->add(dW,"plastic",plastIx,EnergyTracker::IsIncrement | EnergyTracker::ZeroDontCreate,xyz); break;
		}
	}
}

void Law2_L6Geom_LudingPhys::commitWork(const shared_ptr<Contact>& C) {
	auto& ph(C->phys->cast<LudingPhys>());
	assert(scene->trackEnergy);
	if(ph.work.size()<LudingPhys::WORK_SIZE) return;
	// in immediate mode, this was already done in addWork
	if(!wImmediate){
		scene->energy->add(ph.work[LudingPhys::WORK_VISCOUS],"viscous",viscIx,EnergyTracker::IsIncrement | EnergyTracker::ZeroDontCreate,C->geom->node->pos);
		scene->energy->add(ph.work[LudingPhys::WORK_PLASTIC],"plastic",plastIx,EnergyTracker::IsIncrement | EnergyTracker::ZeroDontCreate,C->geom->node->pos);
	}
	const Particle* pA(C->leakPA());
	const Particle* pB(C->leakPB());
	for(const Particle* p: {pA,pB}){
		if(!p->matState) continue;
		boost::mutex::scoped_lock l(p->matState->lock);
		assert(p->matState->isA<LudingMatState>());
		auto& lms=p->matState->cast<LudingMatState>();
		// each particle gets only half
		lms.total+=.5*(ph.work[LudingPhys::WORK_VISCOUS]+ph.work[LudingPhys::WORK_PLASTIC]);
		lms.visco+=.5*(ph.work[LudingPhys::WORK_VISCOUS]);
		lms.plast+=.5*(ph.work[LudingPhys::WORK_PLASTIC]);
	}
}


// use 3 different types to accomodate combinations of Vector2r, Eigen::Map<Vector2r> etc
template<typename VecT1, typename VecT2, typename VecT3>
void Luding_genericSlidingRoutine(Law2_L6Geom_LudingPhys* law, const Real& yieldLim, const Real& k, const Real& visc, const VecT1& vel, VecT2& xi, const Real& dynDivStat, VecT3& resultF, LudingPhys& ph, const Vector3r& xyz){
	static_assert(std::is_same<typename VecT1::Scalar,Real>::value,"Scalar type must be Real");
	const Real& dt(law->scene->dt);
	const bool& trackEnergy(law->scene->trackEnergy);
	// compatible plain dense vector type
	typedef Eigen::Matrix<typename VecT1::Scalar,VecT1::RowsAtCompileTime,1> VecT;
	Real xi0sq(xi.squaredNorm());
	// trial force
	VecT ft=-k*xi-visc*vel;
	if(ft.squaredNorm()<=yieldLim*yieldLim){
		// eq(19), static friction
		xi+=vel*dt;
		resultF=ft;
		// viscous dissipation, only without slip
		if(unlikely(trackEnergy)){
			law->addWork(ph,LudingPhys::WORK_VISCOUS,visc*vel.squaredNorm()*dt,xyz);
		}
	} else {
		// sliding (dynamic) friction norm
		Real fd=yieldLim*dynDivStat;
		Real ftNorm=ft.norm();
		resultF=(ft/ftNorm)*fd;
		if(k>0){
			// only meaningful with non-zero stiffness
			VecT2 xiPrev=xi;
			xi=(-1/k)*(resultF+visc*vel);
			// compute dissipation
		   if(unlikely(trackEnergy)){
				// XXX: sometimes bogus negative value??
				Real W=xiPrev.squaredNorm()-xi.squaredNorm();
				//if(W>0) law->addWork(ph,LudingPhys::WORK_PLASTIC,(1./2)*k*W);
				//if(xiPrev.squaredNorm()<xi.squaredNorm()) cerr<<"<";


				// law->addWork(ph,LudingPhys::WORK_PLASTIC,(1./2)*(ftNorm+fd)*(1./k)*(ftNorm-fd));
				// only consider the plastic part of dissipation; the viscous sliding is fictious
				//VecT f0=k*xiPrev;
				//if(f0.squaredNorm()>yieldLim*yieldLim){
				//	ph.addWork(LudingPhys::WORK_PLASTIC,(1./2)*(1./k)*(f0.squaredNorm()-pow2(fd)));
				//	}
				//VecT f0=k*xiPrev, f1=k*xi;
				//ph.addWork(LudingPhys::WORK_PLASTIC,(1./2)*k*(f0+f1).norm()*(f0-f1).norm());
			}
		} else {
			xi=VecT2::Zero();
		}
	}
	Real xi1sq(xi.squaredNorm());
	// if(xi1sq<
	//ph.addWork(LudingPhys::WORK_PLASTIC,(1./2)*k*(xi1sq-xi0sq));
}


WOO_IMPL_LOGGER(Law2_L6Geom_LudingPhys);

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

	if(g.uN>0){
		// breaking contact, commit dissipation data now
		if(unlikely(scene->trackEnergy)){
			// compute normal plastic work (non-incremental)
			// use previous kn2 and deltaMax, those won't be increased in the step the contact is broken
			const Real& kn2(ph.kn);
			Real delta0=min(ph.deltaLim,ph.deltaMax)*(1-ph.kn1/kn2);
			Real deltaMin=delta0*kn2/(kn2+ph.kna);
			Real Wnp=/*W_np*/(ph.kn1*ph.deltaMax+ph.kna*deltaMin)*delta0;
			addWork(ph,LudingPhys::WORK_PLASTIC,Wnp,g.node->pos);
			commitWork(C);
			// just in case
			//ph.work[LudingPhys::WORK_VISCOUS]=ph.work[LudingPhys::WORK_PLASTIC]=0.;
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
	Real delta0=(1-k1/k2)*ph.deltaMax; // XXX: min(ph.deltaLim,ph.deltaMax)
	// eq (6)
	Real fHys=NaN;
	Real k2dd0=k2*(delta-delta0);
	if(k2dd0>=k1*delta){
		fHys=k1*delta;
		//ph.normBranch=0;
	} else if(-ph.kna*delta>=k2dd0){
		fHys=-ph.kna*delta;
		//ph.normBranch=2;
	} else{
		assert(k1*delta>k2dd0);
		assert(k2dd0>-ph.kna*delta);
		fHys=k2dd0;
		//ph.normBranch=1;
	}
	assert(!isnan(fHys));
	// after eq (8); final normal force
	Real fn=fHys+ph.viscN*(-1.)*velN;
	// force: viscN*velN; distance: velN*Δt
	if(unlikely(scene->trackEnergy)) addWork(ph,LudingPhys::WORK_VISCOUS,ph.viscN*velN*velN*scene->dt,g.node->pos);
	// plastic work computed when contact dissolves

	// TANGENT
	Vector2r ft;
	// coulomb sliding (yield) force
	Real fts=ph.tanPhi*(fn+ph.kna*delta);
	Luding_genericSlidingRoutine(this,fts,ph.kt,ph.viscT,velT,ph.xiT,ph.dynDivStat,ft,ph,g.node->pos);

	// ROLL
	Vector2r mr;
	Real mrs=ph.statR*(fn+ph.kna*delta)*a12;
	Luding_genericSlidingRoutine(this,mrs,ph.kr,ph.viscR,angVelR,ph.xiR,ph.dynDivStat,mr,ph,g.node->pos);

	// TWIST
	Real mw;
	Map1r mw_vec(&mw);
	Real mws=ph.statW*(fn+ph.kna*delta)*a12;
	Map1r xiW_vec(&ph.xiW);
	Luding_genericSlidingRoutine(this,mws,ph.kw,ph.viscW,Map1r_const(&g.angVel[0]),xiW_vec,ph.dynDivStat,mw_vec,ph,g.node->pos);

	// invert signes for applied forces
	Fn=-fn;
	Ft=-ft;
	Tr=-mr;
	Tw=-mw;

	return true;
}




