// 2010 © Václav Šmilauer <eudoxos@arcig.cz>

#include"ParticleContainer.hpp"
#include"Particle.hpp"
#include"ContactLoop.hpp"
#include"Clump.hpp"

#ifdef WOO_OPENMP
	#include<omp.h>
#endif

WOO_PLUGIN(dem,(ParticleContainer));
WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_ParticleContainer__CLASS_BASE_DOC_ATTRS_PY);
WOO_IMPL_LOGGER(ParticleContainer);
 
void ParticleContainer::clear(){
	parts.clear();
	#ifdef WOO_SUBDOMAINS
		subDomains.clear();
	#endif
}

Particle::id_t ParticleContainer::findFreeId(){
	long size=(long)parts.size();
	auto idI=freeIds.begin();
	while(!freeIds.empty()){
		id_t id=(*idI); assert(id>=0);
		idI=freeIds.erase(idI); // remove the current element, advances the iterator
		if(id<=size){
			if(parts[id]) throw std::logic_error("ParticleContainer::findFreeId: freeIds contained "+to_string(id)+", but it is occupied?!");
			return id;
		}
		// if the id is bigger than our size, it could be due to container shrinking, which is ok
	}
	return size; // all particles busy, past-the-end will cause resize
}

#ifdef WOO_SUBDOMAINS
	Particle::id_t ParticleContainer::findFreeDomainLocalId(int subDom){
		#ifdef WOO_OPENMP
			assert(subDom<(int)subDomains.size());
			id_t max=subDomains[subDom].size();
			// LOG_TRACE("subDom={}, max={}",subDom,max);
			id_t& low(subDomainsLowestFree[subDom]);
			for(; low<max; low++){
				if(!(bool)subDomains[subDom][low]) return low;
			}
			return subDomains[subDom].size();
		#else
			assert(false); // this function should never be called in OpenMP-less build
		#endif
	}
#endif

void ParticleContainer::insertAt(shared_ptr<Particle>& p, id_t id){
	assert(id>=0);
	if((size_t)id>=parts.size()){
		std::scoped_lock lock(manipMutex);
		parts.resize(id+1);
	}
	// can be an empty shared_ptr, check needed
	if(p) p->id=id;
	parts[id]=p;
	#ifdef WOO_SUBDOMAINS
		setParticleSubdomain(p,0); // add it to subdomain #0; only effective if subdomains are set up
	#endif
}

Particle::id_t ParticleContainer::insert(shared_ptr<Particle>& p){
	id_t id=findFreeId();
	insertAt(p,id);
	return id;
}

#ifdef WOO_SUBDOMAINS
	void ParticleContainer::clearSubdomains(){ subDomains.clear(); }
	void ParticleContainer::setupSubdomains(){ subDomains.clear(); subDomains.resize(maxSubdomains); }
	// put given parts to 
	bool ParticleContainer::setParticleSubdomain(const shared_ptr<Particle>& b, int subDom){
		#ifdef WOO_OPENMP
			assert(b==parts[b->id]); // consistency check
			// subdomains not used
			if(subDomains.empty()){ b->subDomId=Particle::ID_NONE; return false; }
			// there are subdomains, but it was requested to add the parts to one that does not exists
			// it is an error condition, since traversal of subdomains would siletly skip this parts
			// if(subDom>=(int)subDomains.size()) throw std::invalid_argument(("ParticleContainer::setParticleSubdomain: adding #"+to_string(b->id)+" to non-existent sub-domain "+to_string(subDom)).c_str());
			assert(subDom<(int)subDomains.size());
			id_t localId=findFreeDomainLocalId(subDom);
			if(localId>=(id_t)subDomains[subDom].size()) subDomains[subDom].resize(localId+1);
			subDomains[subDom][localId]=b;
			b->subDomId=domNumLocalId2subDomId(subDom,localId);
			return true;
		#else
			// no subdomains should exist on OpenMP-less builds, and particles should not have subDomId set either
			assert(subDomains.empty()); assert(b->subDomId==Particle::ID_NONE); return false;
		#endif
	}

#endif /* WOO_SUBDOMAINS */

const shared_ptr<Particle>& ParticleContainer::safeGet(Particle::id_t id){
	if(!exists(id)) throw std::invalid_argument("No such particle: #"+to_string(id)+".");
	return (*this)[id];
}


bool ParticleContainer::pyRemove(Particle::id_t id){
	if(!exists(id)) return false;
	dem->removeParticle(id);
	return true;
}

py::list ParticleContainer::pyRemoveList(vector<id_t> ids){
	py::list ret;
	for(auto& id: ids) ret.append(pyRemove(id));
	return ret;
}



bool ParticleContainer::remove(Particle::id_t id){
	if(!exists(id)) return false;
	// this is perhaps not necessary
	std::scoped_lock lock(manipMutex);
	freeIds.push_back(id);
	#ifdef WOO_SUBDOMAINS
		#ifdef WOO_OPENMP
			const shared_ptr<Particle>& b=parts[id];
			if(b->subDomId!=Particle::ID_NONE){
				int subDom, localId;
				std::tie(subDom,localId)=subDomId2domNumLocalId(b->subDomId);
				if(subDom<(int)subDomains.size() && localId<(id_t)subDomains[subDom].size() && subDomains[subDom][localId]==b){
					subDomainsLowestFree[subDom]=min(subDomainsLowestFree[subDom],localId);
				} else {
					LOG_FATAL("Particle::subDomId inconsistency detected for parts #{} while erasing (cross thumbs)!",id);
					if(subDom>=(int)subDomains.size()){ LOG_FATAL("\tsubDomain={} (max {})",subDom,subDomains.size()-1); }
					else if(localId>=(id_t)subDomains[subDom].size()){ LOG_FATAL("\tsubDomain={}, localId={} (max {})",subDom,localId,subDomains[subDom].size()-1); }
					else if(subDomains[subDom][localId]!=b) { LOG_FATAL("\tsubDomains={}, localId={}; erasing #{}, subDomain record points to #{}.",subDom,localId,id,subDomains[subDom][localId]->id); }
				}
			}
		#else
			assert(parts[id]->subDomId==Particle::ID_NONE); // subDomId should never be defined for OpenMP-less builds
		#endif 
	#endif /* WOO_SUBDOMAINS */
	{
		// XXX: see https://svn.boost.org/trac/boost/ticket/8290
		GilLock lock; // avoids the crash :D hopefully removing particles from python will not deadlock
		parts[id].reset(); 
	}
	// removing last element, shrink the size as much as possible
	// extending the vector when the space is allocated is rather efficient
	if((size_t)(id+1)==parts.size()){
		while(id>=0 && !parts[id]) id--; // if the container is completely empty, don't go under zero
		parts.resize(id+1); // if empty, resizes to -1+1=0
	}
	return true;
}


/* python access */
ParticleContainer::pyIterator::pyIterator(ParticleContainer* _pc): pc(_pc), ix(-1){}
shared_ptr<Particle> ParticleContainer::pyIterator::next(){
	int sz=pc->size();
	while(ix<sz){ ix++; if(pc->exists(ix)) return (*pc)[ix]; }
	woo::StopIteration();
	throw; // never reached, but makes the compiler happier
}
ParticleContainer::pyIterator ParticleContainer::pyIterator::iter(){ return *this; }

py::list ParticleContainer::pyFreeIds(){
	py::list ret;
	for(id_t id: freeIds) ret.append(id);
	return ret;
}

Particle::id_t ParticleContainer::pyAppend(shared_ptr<Particle> p, int nodes){
	if(!p) woo::ValueError("Particle to be added is None.");
	if(p->id>=0) IndexError("Particle already has id "+to_string(p->id)+" set; appending such particle (for the second time) is not allowed.");
	if(nodes!=-1 && nodes!=0 && nodes!=1) ValueError("nodes must be ∈ {-1,0,1} (not "+to_string(nodes)+").");
	if(nodes!=0){
		if(!p->shape) woo::ValueError("Particle.shape is None; unable to add nodes.");
		for(const auto& n: p->shape->nodes){
			auto& dyn=n->getData<DemData>();
			// already has an index, check everything is OK
			if(dyn.linIx>=0){
				// node already in DemField.nodes, don't add for the second time
				if(dyn.linIx<(int)dem->nodes.size() && dem->nodes[dyn.linIx].get()==n.get()) continue;
				// node not in DemField.nodes, or says it is somewhere else than it is
				if(dyn.linIx<(int)dem->nodes.size()) std::runtime_error(n->pyStr()+": Node.dem.linIx="+to_string(dyn.linIx)+", but DemField.nodes["+to_string(dyn.linIx)+"]"+(dem->nodes[dyn.linIx]?"="+dem->nodes[dyn.linIx]->pyStr():"is empty (programming error!?)")+".");
				std::runtime_error(n->pyStr()+": Node.dem.linIx="+to_string(dyn.linIx)+", which is out of range for DemField.nodes (size "+to_string(dem->nodes.size())+").");
			} else {
				// maybe add node
				if(nodes==1 || (nodes==-1 && dyn.guessMoving())){
					dyn.linIx=dem->nodes.size();
					dem->nodes.push_back(n);
				}
			}
		}
	}
	return insert(p);
}

py::list ParticleContainer::pyAppendList(vector<shared_ptr<Particle>> pp, int nodes){
	py::list ret;
	for(shared_ptr<Particle>& p: pp){ret.append(pyAppend(p,nodes));}
	return ret;
}

shared_ptr<Node> ParticleContainer::pyAppendClumped(const vector<shared_ptr<Particle>>& pp, const shared_ptr<Node>& clumpNode, int addNodes){
	std::set<void*> seen;
	vector<shared_ptr<Node>> nodes; nodes.reserve(pp.size());
	for(const auto& p:pp){
		pyAppend(p,/*nodes*/false); // do not add nodes for clumps, only the clump node (below) gets added
		for(const auto& n: p->shape->nodes){
			if(seen.count((void*)n.get())!=0) continue;
			seen.insert((void*)n.get());
			nodes.push_back(n);
			if(addNodes){ // add clump nodes to Field.nodes anyway
				n->getData<DemData>().linIx=dem->nodes.size();
				dem->nodes.push_back(n);
			}
		}
	}
	shared_ptr<Node> clump=ClumpData::makeClump(nodes,clumpNode);
	clump->getData<DemData>().linIx=dem->nodes.size();
	dem->nodes.push_back(clump);
	return clump;
}

shared_ptr<Particle> ParticleContainer::pyGetItem(Particle::id_t id){
	if(id<0 && id>=-(int)size()) id+=size();
	if(exists(id)) return (*this)[id];
	woo::IndexError("No such particle: #"+to_string(id)+".");
	return shared_ptr<Particle>(); // make compiler happy
}

ParticleContainer::pyIterator ParticleContainer::pyIter(){ return ParticleContainer::pyIterator(this); }

void ParticleContainer::pyRemask(vector<id_t> ids, int mask, bool visible, bool removeContacts, bool removeOverlapping){
	for(id_t id: ids){
		if(!exists(id)) woo::IndexError("No such particle: #"+to_string(id)+".");
		const auto& p=(*this)[id];
		p->mask=mask;
		p->shape->setVisible(visible);
		if(removeContacts){
			list<id_t> ids2;
			for(const auto& c: p->contacts){
				id_t id2(c.first);
				assert(exists(id2));
				if(!Collider::mayCollide(dem,p,(*this)[id2])) ids2.push_back(id2);
			}
			for(auto id2: ids2){ assert(dem->contacts->find(id,id2)); dem->contacts->remove(dem->contacts->find(id,id2)); }
		}
	}
	// traverse all other particles, check bbox overlaps
	if(removeOverlapping && !parts.empty()){
		list<id_t> toRemove;
		for(const auto& p2: *this){
			if(!p2->shape || !p2->shape->bound){ LOG_DEBUG("#{} has no shape/bound, skipped for overlap check.",p2->id); continue; }
			AlignedBox3r b2(p2->shape->bound->min,p2->shape->bound->max);
			for(id_t id: ids){
				const auto& p=(*this)[id];
				if(!p->shape || !p->shape->bound){ LOG_DEBUG("#{} (being remasked) has no shape/bound, skipped for overlap check.",p2->id); continue; }
				AlignedBox3r b1(p->shape->bound->min,p->shape->bound->max);
				//cerr<<"distance ##"<<id<<"+"<<p2->id<<" is "<<b1.exteriorDistance(b2)<<endl;
				if(b1.exteriorDistance(b2)<=0 && Collider::mayCollide(dem,p2,p)) toRemove.push_back(p2->id);
			}
		}
		for(auto id: toRemove){
			// cerr<<"Removing #"<<id<<endl;
			if(exists(id)) dem->removeParticle(id);
		}
		dem->contacts->dirty=true;
	}
}

