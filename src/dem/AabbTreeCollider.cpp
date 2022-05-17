
#include"AabbTreeCollider.hpp"
#include<boost/range/adaptor/map.hpp>
#include<boost/range/algorithm/copy.hpp>

WOO_PLUGIN(dem,(AabbTreeCollider));
WOO_IMPL_LOGGER(AabbTreeCollider);

WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_AabbTreeCollider__CLASS_BASE_DOC_ATTRS_PY);

// #define ATC_TIMING

#ifdef ATC_TIMING
	#define ATC_CHECKPOINT(cpt) timingDeltas->checkpoint(__LINE__,cpt)
#else
	#define ATC_CHECKPOINT(cpt)
#endif

AlignedBox3r AabbTreeCollider::pyGetAabb(const Particle::id_t& pId){
	if(!tree) woo::RuntimeError("No tree object.");
	if(!tree->hasParticle(pId)) woo::IndexError("No particle {}",pId);
	const auto& aabb(tree->getAABB(pId));
	return AlignedBox3r(aabb.lowerBound,aabb.upperBound);
}

Vector3i AabbTreeCollider::pyGetPeriod(const Particle::id_t& pId){
	if(!tree) woo::RuntimeError("No tree object.");
	if(pId>=boxPeriods.size()) woo::IndexError("No particle {}",pId);
	return boxPeriods[pId];
}

void AabbTreeCollider::invalidatePersistentData(){
	tree.reset();
}

AlignedBox3r AabbTreeCollider::canonicalBox(const Particle::id_t& pId, const Aabb& aabb){
	if(!scene->isPeriodic) return aabb.box;
	assert(boxPeriods.size()>(size_t)pId);
	Vector3i& boxPeriod(boxPeriods[pId]);
	/* handle infinite dimension specially */
	Vector3r cent=aabb.box.center();
	bool wasInf[3]={false,false,false};
	for(int ax=0; ax<3; ax++){
		if(isinf(aabb.box.min()[ax]) || isinf(aabb.box.max()[ax])){ cent[ax]=.5*scene->cell->getSize()[ax]; wasInf[ax]=true; }
	}
	(void)scene->cell->canonicalizePt(cent,boxPeriod);
	Vector3r off=boxPeriod.cast<Real>().array()*scene->cell->getSize().array();
	AlignedBox3r ret(aabb.box.min()-off,aabb.box.max()-off);
	for(int ax=0; ax<3; ax++){
		if(!wasInf[ax]) continue;
		ret.min()[ax]=0;
		ret.max()[ax]=scene->cell->getSize()[ax];
	}
	return ret;
}


void AabbTreeCollider::run(){

	#ifdef ATC_TIMING
		if(!timingDeltas) timingDeltas=make_shared<TimingDeltas>();
		timingDeltas->start();
	#endif


	dem=static_cast<DemField*>(field.get());
	AabbCollider::initBoundDispatcher();
	AabbCollider::setVerletDist(scene,dem);
	assert(verletDist>=0);

	// running the first time, or contacts are dirty and must be detected anew
	if(!tree || dem->contacts->dirty){
		#ifdef WOO_ABBY
			tree=make_shared<abby::tree<Particle::id_t,Vector3r>>();
		#else
			tree=make_shared<::aabb::Tree>(0.0,/*nParticles*/16,/*touchIsOverlap*/true);
		#endif
		dem->contacts->dirty=false;
	}

	// this signals to ContactLoop that we manage overdue contacts
	dem->contacts->stepColliderLastRun=-1;

	#ifdef WOO_ABBY
		if(scene->isPeriodic) throw std::runtime_error("Periodic boundary conditions not handled with WOO_ABBY in AabbTreeCollider.");
	#else
		if(scene->isPeriodic){
			tree->setPeriodicity(::aabb::VecBoolT::Constant(true));
			tree->setBoxSize(scene->cell->getSize());
		} else {
			tree->setPeriodicity(::aabb::VecBoolT::Constant(false));
		}
	#endif

	ATC_CHECKPOINT("bounds:start");
	// parallelizable?
	std::list<Particle::id_t> changed;
	if(scene->isPeriodic) boxPeriods.resize(dem->particles->size());
	for(const shared_ptr<Particle>& p: *dem->particles){
		if(!p->shape) continue;
		assert(dynamic_cast<Aabb*>(p->shape->bound.get()));
		// if(!p->shape->bound) continue;
		#ifdef WOO_ABBY
			if(tree->size()<=p->id){
		#else
			if(tree->nParticles()<=p->id){
		#endif
			AabbCollider::updateAabb(p);
			AlignedBox3r box=canonicalBox(p->id,p->shape->bound->cast<Aabb>());
			#ifdef WOO_ABBY
				tree->insert(p->id,box.min(),box.max());
				LOG_TRACE("   #{}: inserted into the tree (contains {} particles): {}…{}",p->id,tree->size(),box.min().transpose(),box.max().transpose());
			#else
				if(!tree->hasParticle(p->id)) tree->insertParticle(p->id,box.min(),box.max());
				else tree->updateParticle(p->id,box.min(),box.max(),/*alwaysReinsert*/true);
				LOG_TRACE("   #{}: inserted into the tree (contains {} particles): {}…{}",p->id,tree->nParticles(),box.min().transpose(),box.max().transpose());
			#endif
			changed.push_back(p->id);
		}
		else if(AabbCollider::aabbIsDirty(p)){
			AabbCollider::updateAabb(p);
			AlignedBox3r box=canonicalBox(p->id,p->shape->bound->cast<Aabb>());
			LOG_TRACE("   #{}: aabb updated → {}…{}",p->id,box.min().transpose(),box.max().transpose());
			#ifdef WOO_ABBY
				tree->update(p->id,box.min(),box.max(),/*alwaysReinsert*/false);
			#else
				tree->updateParticle(p->id,box.min(),box.max(),/*alwaysReinsert*/true);
			#endif
			changed.push_back(p->id);
		}
	}
	ATC_CHECKPOINT("bounds:end");
	if(changed.empty()){
		LOG_DEBUG("No changed particles, no collision detection necessary.");
		// at every step: remove contacts the contact law requested to be removed
		field->cast<DemField>().contacts->removePending(*this,scene);
		return;
	}

	// tree->rebuild();

	ATC_CHECKPOINT("query:start");
	// TODO: this could be parallelized (useful?)
	for(const auto& idA: changed){
		const auto& pA((*dem->particles)[idA]);
		#ifdef WOO_ABBY
			std::set<Particle::id_t> bbNew;
			tree->query<32>(idA,std::inserter(bbNew,bbNew.begin()));
		#else
			std::vector<unsigned int> bb=tree->query(idA);
			std::set<Particle::id_t> bbNew(bb.begin(),bb.end());
		#endif
		ATC_CHECKPOINT("query:query");
		LOG_DEBUG("#{} has {} new (AABB) contacts:",idA,bbNew.size());
		for(const auto& b: bbNew){
			LOG_TRACE("   {}+{}",idA,b);
			if(!dem->particles->exists(b)){
				LOG_TRACE("   * removing ##{}+{} as {} does not exist.",idA,b,b);
				#ifdef WOO_ABBY
					tree->erase(b);
				#else
					tree->removeParticle(b);
				#endif
			}
		}
		ATC_CHECKPOINT("query:remove-zombies");
		std::set<Particle::id_t> bbOld;
		boost::copy(pA->contacts|boost::adaptors::map_keys,std::inserter(bbOld,bbOld.begin()));
		ATC_CHECKPOINT("query:map-keys");
		LOG_DEBUG("#{} has {} old (DEM) contacts",idA,bbOld.size());
		std::set<Particle::id_t> ccOld, ccNew; // disappeared contacts, newly appeared contacts
		std::set_difference(bbNew.begin(),bbNew.end(),bbOld.begin(),bbOld.end(),std::inserter(ccNew,ccNew.begin()));
		ATC_CHECKPOINT("query:set-fresh");
		std::set_difference(bbOld.begin(),bbOld.end(),bbNew.begin(),bbNew.end(),std::inserter(ccOld,ccOld.begin()));
		ATC_CHECKPOINT("query:set-expired");
		LOG_DEBUG("#{}: {} expired contacts:",idA,bbOld.size());
		ATC_CHECKPOINT("query:compute-sets");
		for(const auto& idB: ccOld){
			LOG_TRACE("   {}+{}",idA,idB);
			const shared_ptr<Contact>& C=dem->contacts->find(idA,idB);
			if(!C){
				// perhaps removed from the "other side" already
				// debug-only
				// if(changed.count(idB)==0) LOG_ERROR("##{}+{} in ccOld & not in contacts & the other unchanged (?!).");
				LOG_TRACE("   * vanished already (deleted from the other side?)");
				continue;
			}
			if(!C->isReal()){
				LOG_DEBUG("   * removing expired non-real contact {}",C->pyStr());
				dem->contacts->removeMaybe_fast(C);
			}
		}
		ATC_CHECKPOINT("query:expired");
		LOG_DEBUG("#{}: {} fresh contacts:",idA,bbNew.size());
		for(const auto& idB: ccNew){
			LOG_TRACE("   {}+{}",idA,idB);
			const auto& pB((*dem->particles)[idB]);
			if(!Collider::mayCollide(dem,pA,pB)){ LOG_TRACE("   * not eligible for collision (Collider::mayCollide)"); continue; }
			if(dem->contacts->find(idA,idB)){
				// perhaps added from the "other side" already
				// debug-only
				// if(changed.count(idB)==0) LOG_ERROR("##{}+{} in ccNew & also existing already & the other not changed (?!)",idA,idB);
				LOG_TRACE("   * appeared already (created from the other side?)");
				continue;
			}
			shared_ptr<Contact> newC=make_shared<Contact>();
			newC->pA=(idA<idB?pA:pB); newC->pB=(idA<idB?pB:pA);
			newC->stepCreated=scene->step;
			newC->stepLastSeen=scene->step;
			if(scene->isPeriodic) newC->cellDist=(boxPeriods[idB]-boxPeriods[idA]);
			dem->contacts->addMaybe_fast(newC); // not thread-safe (ok when not parallelized)
			LOG_TRACE("   * created new contact {}",newC->pyStr());
		}
		ATC_CHECKPOINT("query:fresh");
	}
	ATC_CHECKPOINT("query:end");
	// remove what the contact law wanted to remove
	field->cast<DemField>().contacts->removePending(*this,scene);
	ATC_CHECKPOINT("remove-pending");
}


