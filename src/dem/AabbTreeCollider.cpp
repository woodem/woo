
#include"AabbTreeCollider.hpp"
#include<boost/range/adaptor/map.hpp>
#include<boost/range/algorithm/copy.hpp>

WOO_PLUGIN(dem,(AabbTreeCollider));
WOO_IMPL_LOGGER(AabbTreeCollider);

WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_AabbTreeCollider__CLASS_BASE_DOC_ATTRS);

#define ATC_TIMING

#ifdef ATC_TIMING
	#define ATC_CHECKPOINT(cpt) timingDeltas->checkpoint(__LINE__,cpt)
#else
	#define ATC_CHECKPOINT(cpt)
#endif

void AabbTreeCollider::invalidatePersistentData(){
	tree.reset();
}

void AabbTreeCollider::run(){
	#if 0
		// old test
		::aabb::Tree tt;
		tt.insertParticle(0,Vector3r(-.53,-.53,-.13),Vector3r(.53,.53,.93));
		tt.insertParticle(1,Vector3r(.22,.22,.22),Vector3r(1.78,1.78,1.78));
		tt.insertParticle(2,Vector3r(.27,.97,.97),Vector3r(2.33,3.03,3.03));
		for(int i:{0,1,2}){
			auto q=tt.query(i);
			for(size_t j=0; i<q.size(); i++) LOG_DEBUG("+ ##{}+{}",i,j);
		}
	#endif

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
	for(const shared_ptr<Particle>& p: *dem->particles){
		if(!p->shape) continue;
		// if(!p->shape->bound) continue;
		#ifdef WOO_ABBY
			if(tree->size()<=p->id){
		#else
			if(tree->nParticles()<=p->id){
		#endif
			AabbCollider::updateAabb(p);
			const auto& aabb(p->shape->bound->cast<Aabb>());
			#ifdef WOO_ABBY
				tree->insert(p->id,aabb.box.min(),aabb.box.max());
				LOG_TRACE("   #{}: inserted into the tree (contains {} particles): {}…{}",p->id,tree->size(),aabb.box.min().transpose(),aabb.box.max().transpose());
			#else
				tree->insertParticle(p->id,aabb.box.min(),aabb.box.max());
				LOG_TRACE("   #{}: inserted into the tree (contains {} particles): {}…{}",p->id,tree->nParticles(),aabb.box.min().transpose(),aabb.box.max().transpose());
			#endif
			changed.push_back(p->id);
		}
		else if(AabbCollider::aabbIsDirty(p)){
			AabbCollider::updateAabb(p);
			const auto& aabb(p->shape->bound->cast<Aabb>());
			LOG_TRACE("   #{}: aabb updated",p->id);
			#ifdef WOO_ABBY
				tree->update(p->id,aabb.box.min(),aabb.box.max(),/*alwaysReinsert*/false);
			#else
				tree->updateParticle(p->id,aabb.box.min(),aabb.box.max(),/*alwaysReinsert*/false);
			#endif
			changed.push_back(p->id);
		}
		assert(dynamic_cast<Aabb*>(p->shape->bound.get()));
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


