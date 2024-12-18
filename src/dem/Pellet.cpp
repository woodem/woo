#include"Pellet.hpp"
#include"Sphere.hpp"
#include"Capsule.hpp"
WOO_PLUGIN(dem,(PelletMat)(PelletMatState)(PelletPhys)(Cp2_PelletMat_PelletPhys)(Law2_L6Geom_PelletPhys_Pellet)(PelletCData)(PelletAgglomerator));

WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_PelletMat__CLASS_BASE_DOC_ATTRS_CTOR);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_PelletMatState__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_PelletPhys_CLASS_BASE_DOC_ATTRS_CTOR);
WOO_IMPL__CLASS_BASE_DOC(woo_dem_Cp2_PelletMat_PelletPhys_CLASS_BASE_DOC);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_Law2_L6Geom_PelletPhys_Pellet__CLASS_BASE_DOC_ATTRS_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_PelletCData__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_PelletAgglomerator__CLASS_BASE_DOC_ATTRS);



void Cp2_PelletMat_PelletPhys::go(const shared_ptr<Material>& m1, const shared_ptr<Material>& m2, const shared_ptr<Contact>& C){
	if(!C->phys) C->phys=make_shared<PelletPhys>();
	auto& mat1=m1->cast<PelletMat>(); auto& mat2=m2->cast<PelletMat>();
	auto& ph=C->phys->cast<PelletPhys>();
	Cp2_FrictMat_FrictPhys::updateFrictPhys(mat1,mat2,ph,C);
	ph.normPlastCoeff=.5*(max(0.,mat1.normPlastCoeff)+max(0.,mat2.normPlastCoeff));
	ph.ka=min(max(0.,mat1.kaDivKn),max(0.,mat2.kaDivKn))*ph.kn;
}


// FIXME: energy tracking does not consider adhesion and the integration is very bad (forward trapezoid)
// leading to 20% of energy erro in some cases

WOO_IMPL_LOGGER(Law2_L6Geom_PelletPhys_Pellet);

void Law2_L6Geom_PelletPhys_Pellet::tryAddDissipState(int what, Real E, const shared_ptr<Contact>& C){
	for(const Particle* p: {C->leakPA(), C->leakPB()}){
		if(!p->matState) continue;
		assert(dynamic_cast<PelletMatState*>(p->matState.get()));
		std::scoped_lock l(p->matState->lock);
		auto& pms=p->matState->cast<PelletMatState>();
		switch(what){
			case DISSIP_NORM_PLAST: pms.normPlast+=E/2.; break; 
			case DISSIP_SHEAR_PLAST: pms.shearPlast+=E/2.; break; 
			default: LOG_FATAL("what={}??",what); throw std::logic_error("Invalid what value (programming error)");
		}
	}
}

bool Law2_L6Geom_PelletPhys_Pellet::go(const shared_ptr<CGeom>& cg, const shared_ptr<CPhys>& cp, const shared_ptr<Contact>& C){
	const L6Geom& g(cg->cast<L6Geom>()); PelletPhys& ph(cp->cast<PelletPhys>());
	Real& Fn(ph.force[0]); Eigen::Map<Vector2r> Ft(&ph.force[1]);
	if(C->isFresh(scene)) C->data=make_shared<PelletCData>();
	Real& uNPl(C->data->cast<PelletCData>().uNPl);
	Real& uN0(C->data->cast<PelletCData>().uN0);
	assert(C->data && dynamic_pointer_cast<PelletCData>(C->data));
	if(iniEqlb && C->isFresh(scene)) uN0=g.uN;
	Real uN=g.uN-uN0; // normal displacement, taking iniEqlb in account
	// break contact
	if(uN>0) return false;

	Real d0=g.lens.sum();
	if(ph.normPlastCoeff<=0) uNPl=0.;
	const Vector2r velT(g.vel[1],g.vel[2]);

	ph.torque=Vector3r::Zero();
	
	// normal force
	Fn=ph.kn*(uN-uNPl); // trial force
	if(ph.normPlastCoeff>0){ // normal plasticity activated
		if(Fn>0){
			if(ph.ka<=0) Fn=0;
			else{ Fn=min(Fn,adhesionForce(uN,uNPl,ph.ka)); assert(Fn>0); }
		} else {
			Real Fy=yieldForce(uN,d0,ph.kn,ph.normPlastCoeff);
			// normal plastic slip
			if(Fn<Fy){
				Real uNPl0=uNPl; // needed when tracking energy
				uNPl=uN-Fy/ph.kn;
				if(WOO_UNLIKELY(scene->trackEnergy)){
					// backwards trapezoid integration
					Real Fy0=Fy+yieldForceDerivative(uN,d0,ph.kn,ph.normPlastCoeff)*(uNPl0-uNPl);
					Real dissip=.5*abs(Fy0+Fy)*abs(uNPl-uNPl0);
					scene->energy->add(dissip,plastSplit?"normPlast":"plast",plastSplit?normPlastIx:plastIx,EnergyTracker::IsIncrement | EnergyTracker::ZeroDontCreate);
					tryAddDissipState(DISSIP_NORM_PLAST,dissip,C);
				}
				if(thinRate>0 && thinRelRMin<1.){
					const Vector2r bendVel(g.angVel[1],g.angVel[2]);
					Real dRad_0=thinRate*(uNPl0-uNPl)*(scene->dt*bendVel.norm());
					for(const Particle* p:{C->leakPA(),C->leakPB()}){
						if(!dynamic_cast<Sphere*>(p->shape.get())) continue;
						Real dRad=dRad_0; // copy to be modified
						auto& s=p->shape->cast<Sphere>();
						//Real r0=(C->geom->cast<L6Geom>().lens[p.get()==C->pA.get()?0:1]);
						Real r0=cbrt(3*s.nodes[0]->getData<DemData>().mass/(4*M_PI*p->material->density));
						Real rMin=r0*thinRelRMin;
						if(thinRefRad>0.) rMin*=pow(r0/thinRefRad,thinMinExp);
						if(s.radius<=rMin) continue;
						// 0..1 norm between rMin and r0
						Real r01=(s.radius-rMin)/(r0-rMin);
						if(thinExp>0) dRad*=pow(r01,thinExp);
						if(thinRefRad>0.) dRad*=pow(r0/thinRefRad,thinRateExp);
						std::scoped_lock lock(s.nodes[0]->getData<DemData>().lock);
						// cerr<<"#"<<p->id<<": radius "<<s.radius<<" -> "<<s.radius-dRad<<endl;
						s.radius=max(rMin,s.radius-dRad*r01);
						s.color=CompUtils::clamped(1-(s.radius-rMin)/(r0-rMin),0,1);
					}
				}
				Fn=Fy;
			}
			// in the elastic regime, Fn is trial force already
		}
	}
	/* add fake confinement */
	if(confSigma!=0) Fn-=g.contA*confSigma*(confRefRad>0.?pow(g.contA/(M_PI*pow(confRefRad,2)),confExp):1.);

	// shear force
	Ft+=scene->dt*ph.kt*velT;
	Real maxFt=abs(Fn)*ph.tanPhi; assert(maxFt>=0);
	// shear plastic slip
	if(Ft.squaredNorm()>pow2(maxFt)){
		Real FtNorm=Ft.norm();
		Real ratio=maxFt/FtNorm;
		if(WOO_UNLIKELY(scene->trackEnergy)){
			Real dissip=(.5*(FtNorm-maxFt)+maxFt)*(FtNorm-maxFt)/ph.kt;
			scene->energy->add(dissip,"plast",plastIx,EnergyTracker::IsIncrement | EnergyTracker::ZeroDontCreate);
			tryAddDissipState(DISSIP_SHEAR_PLAST,dissip,C);
		}
		Ft*=ratio;
	}
	assert(!isnan(Fn)); assert(!isnan(Ft[0]) && !isnan(Ft[1]));
	// elastic potential energy
	if(WOO_UNLIKELY(scene->trackEnergy)) scene->energy->add(0.5*(pow2(Fn)/ph.kn+Ft.squaredNorm()/ph.kt),"elast",elastPotIx,EnergyTracker::IsResettable);
	return true;
}


void PelletAgglomerator::run(){
	//DemField& dem(field->cast<DemField>());
	// loop over all source particles, and loop over all contacts of each of them
	if(isnan(massIncPerRad)) throw std::runtime_error("PalletAgglomerator.massIncPerRad==NaN (must be specified)");
	if(dampHalfLife<0) dampHalfLife*=-scene->dt;
	Real sumDMass=0.;
	Real lambda=(dampHalfLife==0 || isnan(dampHalfLife))?0:(log(2)/dampHalfLife);
	for(const shared_ptr<Particle>& src: agglomSrcs){
		for(const auto& idCon: src->contacts){
			const shared_ptr<Contact>& c(idCon.second);
			if(!c->isReal()) continue;
			Particle* other(c->leakOther(src.get()));
			Real* radius=nullptr;
			// if(!other->shape->isA<Sphere>()) continue; // other particles is not a sphere
			if(other->shape->isA<Sphere>()) radius=&(other->shape->cast<Sphere>().radius);
			else if(other->shape->isA<Capsule>()) radius=&(other->shape->cast<Capsule>().radius);
			else continue;
			assert(dynamic_pointer_cast<L6Geom>(c->geom));
			// radius change
			// angVel is local already
			Real dMass=c->geom->cast<L6Geom>().angVel.tail<2>().norm()*scene->dt*massIncPerRad;
			sumDMass+=dMass;
			Real newVol=(4/3.)*M_PI*pow3(*radius)+dMass/other->material->density;
			*radius=cbrt(3*newVol/(4*M_PI));
			other->shape->updateMassInertia(other->material->density);
			if(!other->matState) other->matState=make_shared<PelletMatState>();
			assert(dynamic_pointer_cast<PelletMatState>(other->matState));
			auto& pms=other->matState->cast<PelletMatState>();
			if(pms.stepAgglomUpdated!=scene->step){ pms.agglomRate=0.; pms.stepAgglomUpdated=scene->step; } // reset value
			pms.agglomRate+=dMass/scene->dt;
			// rotation damping
			if(lambda>0){
				other->shape->nodes[0]->getData<DemData>().angVel*=(1-lambda*scene->dt);
			}
			std::scoped_lock l(pms.lock);
			pms.cumAgglomMass+=dMass;
			pms.cumAgglomAngle+=c->geom->cast<L6Geom>().angVel.tail<2>().norm()*scene->dt;
		}
	}
	currRate=(1-currRateSmooth)*currRate+currRateSmooth*(sumDMass/scene->dt);
	mass+=sumDMass;
};
