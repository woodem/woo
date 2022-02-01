#include"Inlet.hpp"
#include"Collision.hpp"
#include"Funcs.hpp"
#include"Clump.hpp"
#include"Sphere.hpp"
#include"Psd.hpp" // PsdSphereGenerator::sanitizePsd

#include"../supp/sphere-pack/SpherePack.hpp"
#include"../supp/smoothing/LinearInterpolate.hpp"

// hack
#include"InsertionSortCollider.hpp"

#include<boost/range/algorithm/lower_bound.hpp>

#include<boost/tuple/tuple_comparison.hpp>

WOO_PLUGIN(dem,(Inlet)(ParticleGenerator)(MinMaxSphereGenerator)(ParticleShooter)(AlignedMinMaxShooter)(RandomInlet)(BoxInlet)(BoxInlet2d)(CylinderInlet)(ArcInlet)(ArcShooter)(SpatialBias)(AxialBias)(PsdAxialBias)(LayeredAxialBias)(NonuniformAxisPlacementBias));

WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_Inlet__CLASS_BASE_DOC_ATTRS_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_ParticleGenerator__CLASS_BASE_DOC_ATTRS_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_MinMaxSphereGenerator_CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC(woo_dem_ParticleShooter__CLASS_BASE_DOC);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_AlignedMinMaxShooter__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_RandomInlet__CLASS_BASE_DOC_ATTRS_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_BoxInlet__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_BoxInlet2d__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_CylinderInlet__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_ArcInlet__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_PY(woo_dem_SpatialBias__CLASS_BASE_DOC_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_AxialBias__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_PsdAxialBias__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_LayeredAxialBias__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_NonuniformAxisPlacementBias__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_ArcShooter__CLASS_BASE_DOC_ATTRS);


WOO_IMPL_LOGGER(RandomInlet);
WOO_IMPL_LOGGER(PsdAxialBias);
WOO_IMPL_LOGGER(LayeredAxialBias);
WOO_IMPL_LOGGER(AxialBias);


void AxialBias::postLoad(AxialBias&,void*){
	if(axis<0 || axis>2) throw std::runtime_error("AxialBias.axis: must be in 0..2 (not "+to_string(axis)+").");
}

Real AxialBias::clampOrApplyPhase(Real p0){
	if(std::isnan(phase)){
		return CompUtils::clamped(p0,0,1);
	}
	Real p1=p0+phase;
	return p1-std::floor(p1);
}

Vector3r AxialBias::unitPos(const Real& d){
	Vector3r p3=Mathr::UnitRandom3();
	if(axis<0 || axis>2) throw std::runtime_error("AxialBias.axis: must be in 0..2 (not "+to_string(axis)+")");
	Real& p(p3[axis]);
	p=clampOrApplyPhase((d-d01[0])/(d01[1]-d01[0])+(p-.5)*fuzz);
	return p3;
}

void PsdAxialBias::postLoad(PsdAxialBias&,void*){
	PsdSphereGenerator::sanitizePsd(psdPts,"PsdAxialBias.psdPts");
	if(!reorder.empty()){
		if(reorder.size()!=psdPts.size()) throw std::runtime_error("PsdAxialBias.reorder: must have the length of psdPts minus 1 (len(reorder)=="+to_string(reorder.size())+", len(psdPts)="+to_string(psdPts.size())+").");
		for(int i=0; i<(int)psdPts.size()-1; i++){
			size_t c=std::count(reorder.begin(),reorder.end(),i);
			if(c!=1) throw std::runtime_error("PsdAxialBias.reorder: must contain all integers in 0.."+to_string(psdPts.size()-1)+", each exactly once ("+to_string(c)+" occurences of "+to_string(i)+").");
		}
	}
}


Vector3r PsdAxialBias::unitPos(const Real& d){
	if(psdPts.empty()) throw std::runtime_error("AxialBias.psdPts: must not be empty.");
	Vector3r p3=Mathr::UnitRandom3();
	Real& p(p3[axis]);
	size_t pos=0; // contains fraction number in the PSD
	if(!discrete){
		p=linearInterpolate(d,psdPts,pos);
	} else {
		Real f0,f1,t;
		std::tie(f0,f1,t)=linearInterpolateRel(d,psdPts,pos);
		LOG_TRACE("PSD interp for {}: {}..{}, pos={}, t={}",d,f0,f1,pos,t);
		if(t==0.){
			// we want the interval to the left of our point
			if(pos==0){ LOG_WARN("PsdAxiaBias.unitPos: discrete PSD interpolation returned point at the beginning for d={}, which should be zero. No interpolation done, setting 0.",d); p=0; return p3; }
			f1=f0; f0=psdPts[pos-1].y();
		}
		// pick randomly in our interval
		p=f0+Mathr::UnitRandom()*(f1-f0);
		//cerr<<"Picked from "<<d0<<".."<<d1<<": "<<p<<endl;
	}
	// reorder fractions if desired
	if(!reorder.empty()){
		Real a=0, dp=p-psdPts[pos].y();
		for(size_t i=0; i<reorder.size(); i++){
			if(reorder[i]==(int)pos){ p=a+dp; break; } 
			// cumulate bin sizes of other fractions, except for the very last one which is zero by definition
			if(i<psdPts.size()-1) a+=psdPts[reorder[i]+1].y()-psdPts[reorder[i]].y();
		}
	}
	// apply fuzz
	p=this->clampOrApplyPhase(p+fuzz*(Mathr::UnitRandom()-.5));
	if(invert) p=1-p;
	return p3;
}

void LayeredAxialBias::postLoad(LayeredAxialBias&,void*){
	// check correctness
	xRangeSum.resize(layerSpec.size());
	for(size_t i=0; i<layerSpec.size(); i++){
		const auto& l=layerSpec[i];
		if(l.size()<4 || l.size()%2!=0) throw std::runtime_error("LayeredAxialBias.layerSpec["+to_string(i)+"]: must have at least 4 items, and even number of items.");
		for(int j=0; j<l.size(); j+=2){
			if(l[j+1]<=l[j]) throw std::runtime_error("LayeredAxialBias.layerSpec["+to_string(i)+"]: items ["+to_string(j)+"]="+to_string(l[j])+" and ["+to_string(j+1)+"]="+to_string(l[j+1])+" not ordered increasingly.");
		}
		for(int j=2; j<l.size(); j++){
			if(!(l[j]>=0 && l[j]<=1)) throw std::runtime_error("LayeredAxialBias.layerSpec["+to_string(i)+"]["+to_string(j)+"]="+to_string(l[j])+": must be in 〈0,1〉.");
		}
		xRangeSum[i]=0;
		for(int j=2; j<l.size(); j+=2) xRangeSum[i]+=l[j+1]-l[j];
	}
}

Vector3r LayeredAxialBias::unitPos(const Real& d){
	Vector3r p3=Mathr::UnitRandom3();
	Real& p(p3[axis]);
	int ll=-1;
	for(size_t i=0; i<layerSpec.size(); i++){ if(layerSpec[i][0]<=d && layerSpec[i][1]>d) ll=i; }
	if(ll<0){ LOG_WARN("No matching fraction for d={}, no bias applied.",d); return p3; }
	const auto& l(layerSpec[ll]);
	Real r=Mathr::UnitRandom()*xRangeSum[ll];
	for(int j=2; j<l.size(); j+=2){
		Real d=l[j+1]-l[j];
		if(r<=d){
			p=l[j]+r;
			p=CompUtils::clamped(p+fuzz*(Mathr::UnitRandom()-.5),0,1); // apply fuzz
			return p3;
		}
		r-=d; 
	}
	LOG_ERROR("internal error: layerSpec[{}]={}: did not select any layer for d={} with xRangeSum[{}]={}; final r={} (original must have been r0={}). What's up? Applying no bias and proceeding.",ll,l.transpose(),d,ll,xRangeSum[ll],r,r+xRangeSum[ll]);
	return p3;
}




#ifdef WOO_OPENGL
	void Inlet::renderMassAndRate(const Vector3r& pos){
		std::ostringstream oss; oss.precision(4); oss<<mass;
		if(maxMass>0){ oss<<"/"; oss.precision(4); oss<<maxMass; }
		if(!isnan(currRate) && !isinf(currRate)){ oss.precision(3); oss<<"\n("<<currRate<<")"; }
		GLUtils::GLDrawText(oss.str(),pos,CompUtils::mapColor(glColor));
	}
#endif


py::object ParticleGenerator::pyPsd(bool mass, bool cumulative, bool normalize, const Vector2r& dRange, const Vector2r& tRange, int num) const {
	if(!save) throw std::runtime_error("ParticleGenerator.save must be True for calling ParticleGenerator.psd()");
	auto tOk=[&tRange](const Real& t){ return isnan(tRange.minCoeff()) || (tRange[0]<=t && t<tRange[1]); };
	vector<Vector2r> psd=DemFuncs::psd(genDiamMassTime,/*cumulative*/cumulative,/*normalize*/normalize,num,dRange,
		/*diameter getter*/[&tOk](const Vector3r& dmt, const size_t& i) ->Real { return tOk(dmt[2])?dmt[0]:NaN; },
		/*weight getter*/[&](const Vector3r& dmt, const size_t& i) -> Real{ return mass?dmt[1]:1.; }
	);// 
	return DemFuncs::seqVectorToPy(psd,[](const Vector2r& i)->Vector2r{ return i; },/*zip*/false);
}

py::object ParticleGenerator::pyDiamMass(bool zipped) const {
	return DemFuncs::seqVectorToPy(genDiamMassTime,/*itemGetter*/[](const Vector3r& i)->Vector2r{ return i.head<2>(); },/*zip*/zipped);
}

Real ParticleGenerator::pyMassOfDiam(Real min, Real max) const{
	Real ret=0.;
	for(const Vector3r& vv: genDiamMassTime){
		if(vv[0]>=min && vv[0]<=max) ret+=vv[1];
	}
	return ret;
};



std::tuple<Real,vector<ParticleGenerator::ParticleAndBox>>
MinMaxSphereGenerator::operator()(const shared_ptr<Material>&mat, const Real& time){
	if(isnan(dRange[0]) || isnan(dRange[1]) || dRange[0]>dRange[1]) throw std::runtime_error("MinMaxSphereGenerator: dRange[0]>dRange[1], or they are NaN!");
	Real r=.5*(dRange[0]+Mathr::UnitRandom()*(dRange[1]-dRange[0]));
	auto sphere=DemFuncs::makeSphere(r,mat);
	Real m=sphere->shape->nodes[0]->getData<DemData>().mass;
	if(save) genDiamMassTime.push_back(Vector3r(2*r,m,time));
	return std::make_tuple(2*r,vector<ParticleAndBox>({{sphere,AlignedBox3r(Vector3r(-r,-r,-r),Vector3r(r,r,r))}}));
};

Real MinMaxSphereGenerator::critDt(Real density, Real young) {
	return DemFuncs::spherePWaveDt(dRange[0],density,young);
}


Real RandomInlet::critDt() {
	if(!generator) return Inf;
	Real ret=Inf;
	for(const auto& m: materials){
		const auto em=dynamic_pointer_cast<ElastMat>(m);
		if(!em) continue;
		ret=min(ret,generator->critDt(em->density,em->young));
	}
	return ret;
}


bool Inlet::everythingDone(){
	if((maxMass>0 && mass>=maxMass) || (maxNum>0 && num>=maxNum)){
		LOG_INFO("mass or number reached, making myself dead.");
		dead=true;
		if(zeroRateAtStop) currRate=0.;
		if(!doneHook.empty()){ LOG_DEBUG("Running doneHook: {}",doneHook); Engine::runPy("Inlet",doneHook); }
		return true;
	}
	return false;
}


void RandomInlet::run(){
	DemField* dem=static_cast<DemField*>(field.get());
	if(!generator) throw std::runtime_error("RandomInlet.generator==None!");
	if(materials.empty()) throw std::runtime_error("RandomInlet.materials is empty!");
	if(collideExisting){
		if(!collider){
		for(const auto& e: scene->engines){ collider=dynamic_pointer_cast<Collider>(e); if(collider) break; }
		if(!collider) throw std::runtime_error("RandomInlet: no Collider found within engines (needed for collisions detection with already existing particles; if you don't need that, set collideExisting=False.)");
		}
		if(dynamic_pointer_cast<InsertionSortCollider>(collider)) static_pointer_cast<InsertionSortCollider>(collider)->forceInitSort=true;
	}
	if(isnan(massRate)) throw std::runtime_error("RandomInlet.massRate must be given (is "+to_string(massRate)+"); if you want to generate as many particles as possible, say massRate=0.");
	if(massRate<=0 && maxAttempts==0) throw std::runtime_error("RandomInlet.massFlowRate<=0 (no massFlowRate prescribed), but RandomInlet.maxAttempts==0. (unlimited number of attempts); this would cause infinite loop.");
	if(maxAttempts<0){
		std::runtime_error("RandomInlet.maxAttempts must be non-negative. Negative value, leading to meaking engine dead, is achieved by setting atMaxAttempts=RandomInlet.maxAttDead now.");
	}
	spheresOnly=generator->isSpheresOnly();
	padDist=generator->padDist();
	if(isnan(padDist) || padDist<0) throw std::runtime_error(generator->pyStr()+".padDist(): returned invalid value "+to_string(padDist));

	// as if some time has already elapsed at the very start
	// otherwise mass flow rate is too big since one particle during Δt exceeds it easily
	// equivalent to not running the first time, but without time waste
	if(stepPrev==-1 && stepPeriod>0) stepPrev=-stepPeriod; 
	long nSteps=scene->step-stepPrev;
	// to be attained in this step;
	stepGoalMass+=massRate*scene->dt*nSteps; // stepLast==-1 if never run, which is OK
	vector<AlignedBox3r> genBoxes; // of particles created in this step
	vector<shared_ptr<Particle>> generated;
	Real stepMass=0.;

	SpherePack spheres;
	//
	if(spheresOnly){
		spheres.pack.reserve(dem->particles->size()*1.2); // something extra for generated particles
		// HACK!!!
		bool isBox=dynamic_cast<BoxInlet*>(this);
		if(isBox && scene->isPeriodic && !scene->cell->hasShear()){
			spheres.cellSize=scene->cell->getSize0();
		}
		AlignedBox3r box;
		if(isBox){ box=this->cast<BoxInlet>().box; }
		for(const auto& p: *dem->particles){
			if(!p->shape->isA<Sphere>()) continue;
			Vector3r pos(p->shape->nodes[0]->pos);
			if(scene->isPeriodic) pos=scene->cell->canonicalizePt(pos);
			const auto& radius(p->shape->cast<Sphere>().radius);
			// AlignedBox::squaredExteriorDistance returns 0 for points inside the box
			if(isBox && box.squaredExteriorDistance(pos)>pow2(radius)) continue;
			spheres.pack.push_back(SpherePack::Sph(pos,radius));
		}
		if(isBox && scene->isPeriodic && !scene->cell->hasShear()){
			// cerr<<"Size w/o shadows: "<<spheres.pack.size()<<endl;
			spheres.addShadows();
			// cerr<<"Size  w/ shadows: "<<spheres.pack.size()<<endl;
		}
	}

	#if 0
		if(massRate<=0) Master::instance().checkApi(/*minApi*/10103,"RandomInlet.massRate<=0 newly honors RandomInlet.atMaxAttempts; to restore the old behavior, say atMaxAttempts='silent'.",/*pyWarn*/false);
	#endif

	while(true){
		// finished forever
		if(everythingDone()) return;

		// finished in this step
		if(massRate>0 && mass>=stepGoalMass) break;

		shared_ptr<Material> mat;
		shared_ptr<MatState> matState;
		Vector3r pos=Vector3r::Zero();
		Real diam;
		vector<ParticleGenerator::ParticleAndBox> pee;
		int attempt=-1;
		while(true){
			attempt++;
			/***** too many tries, give up ******/
			if(attempt>=maxAttempts){
				generator->revokeLast(); // last particle could not be placed
				if(massRate<=0){
					LOG_DEBUG("maxAttempts={} reached; since massRate is not positive, we're done in this step",maxAttempts);
					goto stepDone;
				}
				switch(atMaxAttempts){
					case MAXATT_ERROR: throw std::runtime_error("RandomInlet.maxAttempts reached ("+to_string(maxAttempts)+")"); break;
					case MAXATT_DEAD:{
						LOG_INFO("maxAttempts={} reached, making myself dead.",maxAttempts);
						this->dead=true;
						return;
					}
					case MAXATT_WARN: LOG_WARN("maxAttempts {} reached before required mass amount was generated; continuing, since maxAttemptsError==False",maxAttempts); break;
					case MAXATT_SILENT: break;
					case MAXATT_DONE: Engine::runPy("RandomInlet",doneHook); break;
					default: throw std::invalid_argument("Invalid value of RandomInlet.atMaxAttempts="+to_string(atMaxAttempts)+".");
				}
				goto stepDone;
			}
			/***** each maxAttempts/attPerPar, try a new particles *****/	
			// the first condition after || guards against FPE in attempt%0
			if(attempt==0 || ((maxAttempts/attemptPar>0) && (attempt%(maxAttempts/attemptPar))==0)){
				LOG_DEBUG("attempt {}: trying with a new particle.",attempt);
				if(attempt>0) generator->revokeLast(); // if not at the beginning, revoke the last particle

				// random choice of material with equal probability
				size_t matIx=0;
				assert(materials.size()>=1); // was already checked above
				if(materials.size()>1) matIx=max(size_t(materials.size()*Mathr::UnitRandom()),materials.size()-1);
				mat=materials[matIx];
				if(matIx<matStates.size() && matStates[matIx]) matState=static_pointer_cast<MatState>(Master::instance().deepcopy(matStates[matIx]));
				else matState=shared_ptr<MatState>();
				// generate a new particle
				std::tie(diam,pee)=(*generator)(mat,scene->time);
				assert(!pee.empty());
				LOG_TRACE("Placing {}-sized particle; first component is a {}, extents from {} to {}",pee.size(),pee[0].par->getClassName(),pee[0].extents.min().transpose(),pee[0].extents.max().transpose());
				// all particles in the clump will share a single matState instance
				if(matState){
					for(auto& pe: pee) pe.par->matState=matState;
				}
			}

			pos=randomPosition(diam,padDist); // overridden in child classes
			LOG_TRACE("Trying pos={}",pos.transpose());
			for(const auto& pe: pee){
				// make translated copy
				AlignedBox3r peBox(pe.extents); peBox.translate(pos); 
				// box is not entirely within the factory shape
				if(!validateBox(peBox)){ LOG_TRACE("validateBox failed."); goto tryAgain; }

				const shared_ptr<woo::Sphere>& peSphere=dynamic_pointer_cast<woo::Sphere>(pe.par->shape);

				if(spheresOnly){
					if(!peSphere) throw std::runtime_error("RandomInlet.spheresOnly: is true, but a nonspherical particle ("+pe.par->shape->pyStr()+") was returned by the generator.");
					Real r=peSphere->radius;
					Vector3r subPos=peSphere->nodes[0]->pos;
					for(const auto& s: spheres.pack){
						// check dist && don't collide with another sphere from this clump
						// (abuses the *num* counter for clumpId)
						if((s.c-(pos+subPos)).squaredNorm()<pow2(s.r+r)){
							LOG_TRACE("Collision with a particle in SpherePack (a particle generated in this step).");
							goto tryAgain;
						}
					}
					// don't add to spheres until all particles will have been checked for overlaps (below)
				} else {
					// see intersection with existing particles
					bool overlap=false;
					if(collideExisting){
						vector<Particle::id_t> ids=collider->probeAabb(peBox.min(),peBox.max());
						for(const auto& id: ids){
							LOG_TRACE("Collider reports intersection with #{}",id);
							if(id>(Particle::id_t)dem->particles->size() || !(*dem->particles)[id]) continue;
							const shared_ptr<Shape>& sh2((*dem->particles)[id]->shape);
							// no spheres, or they are too close
							if(!peSphere || !sh2->isA<woo::Sphere>() || 1.1*(pos-sh2->nodes[0]->pos).squaredNorm()<pow2(peSphere->radius+sh2->cast<Sphere>().radius)) goto tryAgain;
						}
					}

					// intersection with particles generated by ourselves in this step
					for(size_t i=0; i<genBoxes.size(); i++){
						overlap=(genBoxes[i].squaredExteriorDistance(peBox)==0.);
						if(overlap){
							const auto& genSh(generated[i]->shape);
							// for spheres, try to compute whether they really touch
							if(!peSphere || !genSh->isA<Sphere>() || (pos-genSh->nodes[0]->pos).squaredNorm()<pow2(peSphere->radius+genSh->cast<Sphere>().radius)){
								LOG_TRACE("Collision with {}-th particle generated in this step.",i);
								goto tryAgain;
							}
						}
					}
				}
			}
			LOG_DEBUG("No collision (attempt {}), particle will be created :-) ",attempt);
			if(spheresOnly){
				// num will be the same for all spheres within this clump (abuse the *num* counter)
				for(const auto& pe: pee){
					Vector3r subPos=pe.par->shape->nodes[0]->pos;
					Real r=pe.par->shape->cast<Sphere>().radius;
					spheres.pack.push_back(SpherePack::Sph((pos+subPos),r,/*clumpId*/(pee.size()==1?-1:num)));
					// add shadow particles for periodic conditions
					// FIXME: this should be only done if the shadow particle is relevant for the inlet box??
					if(scene->isPeriodic){
						size_t iLast=spheres.pack.size()-1;
						Vector3r p0(scene->cell->canonicalizePt(pos+subPos));
						const Vector3r& cellSize(spheres.cellSize);
						AlignedBox3r box(Vector3r::Zero(),cellSize);
						for(int xx:{-1,0,1}) for(int yy:{-1,0,1}) for (int zz:{-1,0,1}){
							if(xx==0 && yy==0 && zz==0) continue;
							Vector3r p=p0+cellSize.cwiseProduct(Vector3r(xx,yy,zz));
							if(box.squaredExteriorDistance(p)<r){
								spheres.pack.push_back(SpherePack::Sph(p,r,/*clumpId*/(pee.size()==1?-1:num),/*shadowOf*/iLast));
							}
						}
					}
				}
			}
			break;
			tryAgain: ; // try to position the same particle again
		}

		// particle was generated successfully and we have place for it
		for(const auto& pe: pee){
			genBoxes.push_back(AlignedBox3r(pe.extents).translate(pos));
			generated.push_back(pe.par);
		}

		num+=1;

		#ifdef WOO_OPENGL			
			Real color_=isnan(color)?Mathr::UnitRandom():color;
		#endif
		if(pee.size()>1){ // clump was generated
			//LOG_WARN("Clumps not yet tested properly.");
			vector<shared_ptr<Node>> nn;
			for(auto& pe: pee){
				auto& p=pe.par;
				p->mask=mask;
				#ifdef WOO_OPENGL
					assert(p->shape);
					if(color_>=0) p->shape->color=color_; // otherwise keep generator-assigned color
				#endif
				if(p->shape->nodes.size()!=1) LOG_WARN("Adding suspicious clump containing particle with more than one node (please check, this is perhaps not tested");
				for(const auto& n: p->shape->nodes){
					nn.push_back(n);
					n->pos+=pos;
				}
				dem->particles->insert(p);
			}
			shared_ptr<Node> clump=ClumpData::makeClump(nn,/*no central node pre-given*/shared_ptr<Node>(),/*intersection*/false);
			auto& dyn=clump->getData<DemData>();
			if(shooter) (*shooter)(clump);
			if(scene->trackEnergy) scene->energy->add(-DemData::getEk_any(clump,true,true,scene),"kinInlet",kinEnergyIx,EnergyTracker::ZeroDontCreate|EnergyTracker::IsIncrement,clump->pos);
			if(dyn.angVel!=Vector3r::Zero()){
				throw std::runtime_error("pkg/dem/RandomInlet.cpp: generated particle has non-zero angular velocity; angular momentum should be computed so that rotation integration is correct, but it was not yet implemented.");
				// TODO: compute initial angular momentum, since we will (very likely) use the aspherical integrator
			}
			ClumpData::applyToMembers(clump,/*reset*/false); // apply velocity
			#ifdef WOO_OPENGL
				std::scoped_lock lock(dem->nodesMutex);
			#endif
			dyn.linIx=dem->nodes.size();
			dem->nodes.push_back(clump);

			mass+=clump->getData<DemData>().mass;
			stepMass+=clump->getData<DemData>().mass;
		} else {
			auto& p=pee[0].par;
			p->mask=mask;
			assert(p->shape);
			#ifdef WOO_OPENGL
				if(color_>=0) p->shape->color=color_;
			#endif
			assert(p->shape->nodes.size()==1); // if this fails, enable the block below
			const auto& node0=p->shape->nodes[0];
			assert(node0->pos==Vector3r::Zero());
			node0->pos+=pos;
			auto& dyn=node0->getData<DemData>();
			if(shooter) (*shooter)(node0);
			if(scene->trackEnergy) scene->energy->add(-DemData::getEk_any(node0,true,true,scene),"kinInlet",kinEnergyIx,EnergyTracker::ZeroDontCreate|EnergyTracker::IsIncrement,node0->pos);
			mass+=dyn.mass;
			stepMass+=dyn.mass;
			assert(node0->hasData<DemData>());
			dem->particles->insert(p);
			#ifdef WOO_OPENGL
				std::scoped_lock lock(dem->nodesMutex);
			#endif
			dyn.linIx=dem->nodes.size();
			dem->nodes.push_back(node0);
			// handle multi-nodal particle (unused now)
			#if 0
				// TODO: track energy of the shooter
				// in case the particle is multinodal, apply the same to all nodes
				if(shooter) shooter(p.shape->nodes[0]);
				const Vector3r& vel(p->shape->nodes[0]->getData<DemData>().vel); const Vector3r& angVel(p->shape->nodes[0]->getData<DemData>().angVel);
				for(const auto& n: p.shape->nodes){
					auto& dyn=n->getData<DemData>();
					dyn.vel=vel; dyn.angVel=angVel;
					mass+=dyn.mass;

					n->linIx=dem->nodes.size();
					dem->nodes.push_back(n);
				}
			#endif
		}
	};

	stepDone:
	setCurrRate(stepMass/(nSteps*scene->dt));
}

Vector3r BoxInlet::randomPosition(const Real& rad, const Real& padDist){
	AlignedBox3r box2(box.min()+padDist*Vector3r::Ones(),box.max()-padDist*Vector3r::Ones());
	Vector3r pos;
	if(!spatialBias) pos=box2.sample();
	else pos=box2.min()+spatialBias->unitPos(rad).cwiseProduct(box2.sizes());
	if(!node) return pos;
	return node->loc2glob(pos);
}



#ifdef WOO_OPENGL
	void BoxInlet::render(const GLViewInfo&) {
		if(isnan(glColor)) return;
		glPushMatrix();
			if(node) GLUtils::setLocalCoords(node->pos,node->ori);
			GLUtils::AlignedBox(box,CompUtils::mapColor(glColor));
			renderMassAndRate(box.center());
		glPopMatrix();
	}
#endif


#ifdef BOX_FACTORY_PERI
bool BoxInlet::validatePeriodicBox(const AlignedBox3r& b) const {
	if(periSpanMask==0) return box.contains(b);
	// otherwise just enlarge our box in all directions
	AlignedBox3r box2(box);
	for(int i:{0,1,2}){
		if(periSpanMask&(1<<i)) continue;
		Real extra=b.sizes()[i]/2.;
		box2.min()[i]-=extra; box2.max()[i]+=extra;
	}
	return box2.contains(b);
}
#endif

#ifdef WOO_OPENGL
	void CylinderInlet::render(const GLViewInfo&){
		if(isnan(glColor) || !node) return;
		GLUtils::Cylinder(node->loc2glob(Vector3r::Zero()),node->loc2glob(Vector3r(height,0,0)),radius,CompUtils::mapColor(glColor),/*wire*/true,/*caps*/false,/*rad2: use rad1*/-1,/*slices*/glSlices);
		Inlet::renderMassAndRate(node->loc2glob(Vector3r(height/2,0,0)));
	}
#endif


Vector3r CylinderInlet::randomPosition(const Real& rad, const Real& padDist) {
	if(!node) throw std::runtime_error("CylinderInlet.node==None.");
	if(padDist>radius) throw std::runtime_error("CylinderInlet.randomPosition: padDist="+to_string(padDist)+" > radius="+to_string(radius));
	// http://stackoverflow.com/questions/5837572/generate-a-random-point-within-a-circle-uniformly
	Vector3r rand=(spatialBias?spatialBias->unitPos(rad):Mathr::UnitRandom3());
	Real t=2*M_PI*rand[0];
	Real u=2*rand[1];
	Real r=u>1.?2.-u:u;
	Real h=rand[2]*(height-2*padDist)+padDist;
	Vector3r p(h,(radius-padDist)*r*cos(t),(radius-padDist)*r*sin(t));
	return node->loc2glob(p);

};

bool CylinderInlet::validateBox(const AlignedBox3r& b) {
	if(!node) throw std::runtime_error("CylinderInlet.node==None.");
	/* check all corners are inside the cylinder */
	#if 0
		boxesTried.push_back(b);
	#endif
	for(AlignedBox3r::CornerType c:{AlignedBox3r::BottomLeft,AlignedBox3r::BottomRight,AlignedBox3r::TopLeft,AlignedBox3r::TopRight,AlignedBox3r::BottomLeftCeil,AlignedBox3r::BottomRightCeil,AlignedBox3r::TopLeftCeil,AlignedBox3r::TopRightCeil}){
		Vector3r p=node->glob2loc(b.corner(c));
		if(p[0]<0. || p[0]>height) return false;
		if(Vector2r(p[1],p[2]).squaredNorm()>pow2(radius)) return false;
	}
	return true;
}


void ArcInlet::postLoad(ArcInlet&, void* attr){
	if(!cylBox.isEmpty() && (cylBox.min()[0]<0 || cylBox.max()[0]<0)) throw std::runtime_error("ArcInlet.cylBox: radius bounds (x-component) must be non-negative (not "+to_string(cylBox.min()[0])+".."+to_string(cylBox.max()[0])+").");
	if(!node){ node=make_shared<Node>(); throw std::runtime_error("ArcInlet.node: must not be None (dummy node created)."); }
};


Vector3r ArcInlet::randomPosition(const Real& rad, const Real& padDist) {
	AlignedBox3r b2(cylBox);
	Vector3r pad(padDist,padDist/cylBox.min()[0],padDist); b2.min()+=pad; b2.max()-=pad;
	if(!spatialBias) return node->loc2glob(CompUtils::cylCoordBox_sample_cartesian(b2));
	else return node->loc2glob(CompUtils::cylCoordBox_sample_cartesian(b2,spatialBias->unitPos(rad)));
}

bool ArcInlet::validateBox(const AlignedBox3r& b) {
	for(const auto& c:{AlignedBox3r::BottomLeftFloor,AlignedBox3r::BottomRightFloor,AlignedBox3r::TopLeftFloor,AlignedBox3r::TopRightFloor,AlignedBox3r::BottomLeftCeil,AlignedBox3r::BottomRightCeil,AlignedBox3r::TopLeftCeil,AlignedBox3r::TopRightCeil}){
		// FIXME: all boxes fail?!
		if(!CompUtils::cylCoordBox_contains_cartesian(cylBox,node->glob2loc(b.corner(c)))) return false;
	}
	return true;
}

#ifdef WOO_OPENGL
	void ArcInlet::render(const GLViewInfo&) {
		if(isnan(glColor)) return;
		glPushMatrix();
			GLUtils::setLocalCoords(node->pos,node->ori);
			GLUtils::RevolvedRectangle(cylBox,CompUtils::mapColor(glColor),glSlices);
		glPopMatrix();
		Inlet::renderMassAndRate(node->loc2glob(CompUtils::cyl2cart(cylBox.center())));
	}
#endif


void ArcShooter::postLoad(ArcShooter&, void*){
	if(!node){ node=make_shared<Node>(); throw std::runtime_error("ArcShooter.node: must not be None (dummy node created)."); }
}

Vector3r cart2spher(const Vector3r& xyz){
	Real r=xyz.norm();
	Real azim=atan2(xyz[1],xyz[0]);
	Real elev=(r==0.?0.:asin(xyz[2]/r));
	return Vector3r(r,azim,elev);
}

Vector3r spher2cart(const Vector3r& rAzimElev){
	const auto& r(rAzimElev[0]); const auto& azim(rAzimElev[1]); const auto& elev(rAzimElev[2]);
	return r*Vector3r(cos(elev)*cos(azim),cos(elev)*sin(azim),sin(elev));
}

void ArcShooter::operator()(const shared_ptr<Node>& n){
	// local coords where x is radial WRT cylindrical coords defined by *node*
	Vector3r rPhiZ(CompUtils::cart2cyl(node->ori.conjugate()*(n->pos-node->pos)));
	Quaternionr qPerp=Quaternionr(AngleAxisr(rPhiZ[1],Vector3r::UnitZ()));
	// Quaternion qElev=AngleAxisr(atan2(rPhiZ[2],rPhiZ[0]),Vector3r::UnitY);
	Real vNorm=Mathr::IntervalRandom(vRange[0],vRange[1]);
	Real azim=Mathr::IntervalRandom(azimRange[0],azimRange[1]);
	Real elev=Mathr::IntervalRandom(elevRange[0],elevRange[1]);
	n->getData<DemData>().vel=node->ori*qPerp*spher2cart(Vector3r(/*radius*/vNorm,/*azimuth*/azim,/*elev*/elev));
}


void NonuniformAxisPlacementBias::postLoad(NonuniformAxisPlacementBias&,void* attr){
	if(!pdf.empty()){
		for(const Real& v: pdf){if(!(v>=0) || isinf(v)) throw std::runtime_error("NonuniformAxisPlacementBias.pdf: contains negative or denormalized numbers."); }
		cdf.resize(pdf.size());
		Real _dx=1./pdf.size();
		cdf[0]=0.;
		for(size_t i=1; i<pdf.size(); i++) cdf[i]=cdf[i-1]+_dx*.5*(pdf[i]+pdf[i-1]);
	}
	if(!cdf.empty()){
		// cerr<<"CDF"<<endl; for(const auto& c: cdf) cerr<<c<<endl;
		if((!(cdf.back()>0)) || isinf(cdf.back())) throw std::runtime_error("NonuniformAxisPlacementBias.cdf: last cumulated value is not positive (or is infinite), unable to normalize.");
		for(auto& c: cdf) c/=cdf.back(); // normalize cdf
		// cerr<<"CDF normalized"<<endl; for(const auto& c: cdf) cerr<<c<<endl;
		dx=1./cdf.size();
	}
	if(axis<0 || axis>2) throw std::runtime_error("NonuniformAxisPlacementBias.axis: must be in 0..2 (not "+to_string(axis)+")");
}

Vector3r NonuniformAxisPlacementBias::unitPos(const Real& diam){
	if(cdf.empty()) throw std::runtime_error("NonuniformAxisPlacementBias.cdf is empty.");
	Vector3r p3=Mathr::UnitRandom3();
	Real& p(p3[axis]);
	// lower_bound computes lowest i such that cdf[i]>=p
	size_t i=std::lower_bound(cdf.begin(),cdf.end(),p)-cdf.begin();
	if(i==0) return p3; // p is also zero
	Real mezzo=(p-cdf[i-1])/(cdf[i]-cdf[i-1]); // where are we in-between points on unit scale
	if(cdf[i]==cdf[i-1]) mezzo=0; // just in case
	p=((i-1)+mezzo)*dx;
	return p3;
};

