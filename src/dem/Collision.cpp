#include<woo/pkg/dem/Collision.hpp>
#include<woo/pkg/dem/ParticleContainer.hpp>
#include<woo/pkg/dem/Contact.hpp>
#ifdef WOO_OPENGL
	#include<woo/lib/opengl/OpenGLWrapper.hpp>
#endif

WOO_PLUGIN(dem,(Aabb)(BoundFunctor)(BoundDispatcher)(Collider)(AabbCollider));
WOO_IMPL_LOGGER(AabbCollider);

WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_Aabb__CLASS_BASE_DOC_ATTRS_CTOR);
WOO_IMPL__CLASS_BASE_DOC_PY(woo_dem_BoundFunctor__CLASS_BASE_DOC_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_Collider__CLASS_BASE_DOC_ATTRS_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_AabbCollider__CLASS_BASE_DOC_ATTRS);

#ifdef WOO_OPENGL
	WOO_PLUGIN(gl,(Gl1_Aabb))
	WOO_IMPL__CLASS_BASE_DOC(woo_dem_Gl1_Aabb__CLASS_BASE_DOC);
#endif

bool Collider::mayCollide(const DemField* dem, const shared_ptr<Particle>& pA, const shared_ptr<Particle>& pB){
	/* particles which share nodes may not collide */
	if(!pA || !pB || !pA->shape || !pB->shape || pA.get()==pB.get()) return false;
	/* particles not shaing mask may not collide */
	if(!(pA->mask&pB->mask)) return false;
	/* particles sharing bits in loneMask may not collide */
	if((pA->mask&pB->mask&dem->loneMask)!=0) return false;
	/* mix and match all nodes with each other (this may be expensive, hence after easier checks above) */
	const auto& nnA(pA->shape->nodes); const auto& nnB(pB->shape->nodes);
	for(const auto& nA: nnA) for(const auto& nB: nnB) {
		// particles share a node
		if(nA.get()==nB.get()) return false; 
		// particles are inside the same clump?
		assert(nA->hasData<DemData>() && nB->hasData<DemData>());
		const auto& dA(nA->getData<DemData>()); const auto& dB(nB->getData<DemData>());
		if(dA.isClumped() && dB.isClumped()){
			assert(dA.master.lock() && dB.master.lock());
			if(dA.master.lock().get()==dB.master.lock().get()) return false;
		}
	}
	// in other cases, do collide
	return true;
}

void BoundDispatcher::run(){
	updateScenePtr();
	for(const shared_ptr<Particle>& p: *(field->cast<DemField>().particles)){
		shared_ptr<Shape>& shape=p->shape;
		if(!shape) continue;
		operator()(shape);
		if(!shape) continue; // the functor did not create new bound
	#if 0
		Aabb* aabb=WOO_CAST<Aabb*>(shape->bound.get());
		Real sweep=velocityBins->bins[velocityBins->bodyBins[p->id]].maxDist;
		aabb->min-=Vector3r(sweep,sweep,sweep);
		aabb->max+=Vector3r(sweep,sweep,sweep);
	#endif
	}
}


void AabbCollider::pyHandleCustomCtorArgs(py::args_& t, py::kwargs& d){
	if(py::len(t)==0) return; // nothing to do
	if(py::len(t)!=1) throw invalid_argument("AabbCollider optionally takes exactly one list of BoundFunctor's as non-keyword argument for constructor ("+to_string(py::len(t))+" non-keyword ards given instead)");
	if(!boundDispatcher) boundDispatcher=make_shared<BoundDispatcher>();
	vector<shared_ptr<BoundFunctor>> vf=py::extract<vector<shared_ptr<BoundFunctor>>>((t[0]))();
	for(const auto& f: vf) boundDispatcher->add(f);
	t=py::tuple(); // empty the args
}

void AabbCollider::getLabeledObjects(const shared_ptr<LabelMapper>& labelMapper){ if(boundDispatcher) boundDispatcher->getLabeledObjects(labelMapper); Engine::getLabeledObjects(labelMapper); }

void AabbCollider::initBoundDispatcher(){
	if(!boundDispatcher) throw std::runtime_error(fmt::format("AabbCollider.boundDispatcher is None ({}).",this->pyStr()));
	boundDispatcher->scene=scene;
	boundDispatcher->field=field;
	boundDispatcher->updateScenePtr();
}

bool AabbCollider::aabbIsDirty(const shared_ptr<Particle>& p){
	const int nNodes=p->shape->nodes.size();
	// below we throw exception for particle that has no functor afer the dispatcher has been called
	// that would prevent mistakenly boundless particless triggering collisions every time
	if(!p->shape->bound){
		LOG_TRACE("recomputeBounds because of #{} without bound",p->id);
		return true;
	}
	// existing bound, do we need to update it?
	const Aabb& aabb=p->shape->bound->cast<Aabb>();
	assert(aabb.nodeLastPos.size()==p->shape->nodes.size());
	if(isnan(aabb.min.maxCoeff())||isnan(aabb.max.maxCoeff())) { return true; }
	// check rotation difference, for particles where it matters
	Real moveDueToRot2=0.;
	if(aabb.maxRot>=0.){
		assert(!isnan(aabb.maxRot));
		Real maxRot=0.;
		for(int i=0; i<nNodes; i++){
			AngleAxisr aa(aabb.nodeLastOri[i].conjugate()*p->shape->nodes[i]->ori);
			// moving will decrease the angle, it is taken in account here, with the asymptote
			// it is perhaps not totally correct... :|
			maxRot=max(maxRot,abs(aa.angle())); // abs perhaps not needed?
			//cerr<<"#"<<p->id<<": rot="<<aa.angle()<<" (max "<<aabb.maxRot<<")"<<endl;
		}
		if(maxRot>aabb.maxRot){
			LOG_TRACE("recomputeBounds because of #{} rotating too much",p->id);
			return true;
		}
		// linearize here, but don't subtract verletDist
		moveDueToRot2=pow2(.5*(aabb.max-aabb.min).maxCoeff()*maxRot);
	}
	// check movement
	Real d2=0; 
	for(int i=0; i<nNodes; i++){
		d2=max(d2,(aabb.nodeLastPos[i]-p->shape->nodes[i]->pos).squaredNorm());
		//cerr<<"#"<<p->id<<": move2="<<d2<<" (+"<<moveDueToRot2<<"; max "<<aabb.maxD2<<")"<<endl;
		// maxVel2b=max(maxVel2b,p->shape->nodes[i]->getData<DemData>().vel.squaredNorm());
	}
	if(d2+moveDueToRot2>aabb.maxD2){
		LOG_TRACE("recomputeBounds because of #{} moved too far",p->id);
		return true;
	}
	return false;
}

void AabbCollider::updateAabb(const shared_ptr<Particle>& p){
	if(!p || !p->shape) return;
	// call dispatcher now
	// cerr<<"["<<p->id<<"]";
	boundDispatcher->operator()(p->shape);
	if(!p->shape->bound){
		if(noBoundOk) return;
		throw std::runtime_error("InsertionSortCollider: No bound was created for #"+to_string(p->id)+", provide a Bo1_*_Aabb functor for it. (Particle without Aabb are not supported yet, and perhaps will never be (what is such a particle good for?!)");
	}
	Aabb& aabb=p->shape->bound->cast<Aabb>();
	const int nNodes=p->shape->nodes.size();
	// save reference node positions
	aabb.nodeLastPos.resize(nNodes);
	aabb.nodeLastOri.resize(nNodes);
	for(int i=0; i<nNodes; i++){
		aabb.nodeLastPos[i]=p->shape->nodes[i]->pos;
		aabb.nodeLastOri[i]=p->shape->nodes[i]->ori;
	}
	aabb.maxD2=pow2(verletDist);
	if(isnan(aabb.maxRot)) throw std::runtime_error("S.dem.par["+to_string(p->id)+"]: bound functor did not set maxRot -- should be set to either to a negative value (to ignore it) or to non-negative value (maxRot will be set from verletDist in that case); this is an implementation error.");
	#if 0
		// proportionally to relVel, shift bbox margin in the direction of velocity
		// take velocity of nodes[0] as representative
		const Vector3r& v0=p->shape->nodes[0]->getData<DemData>().vel;
		Real vNorm=v0.norm();
		Real relVel=max(maxVel2b,maxVel2)==0?0:vNorm/sqrt(max(maxVel2b,maxVel2));
	#endif
	if(verletDist>0){
		if(aabb.maxRot>=0){
			// maximum rotation arm, assume centroid in the middle
			Real maxArm=.5*(aabb.max-aabb.min).maxCoeff();
			if(maxArm>0.) aabb.maxRot=atan(verletDist/maxArm); // FIXME: this may be very slow...?
			else aabb.maxRot=0.;
		}
		aabb.max+=verletDist*Vector3r::Ones();
		aabb.min-=verletDist*Vector3r::Ones();
	}
}

void AabbCollider::setVerletDist(Scene* scene, DemField* dem){
	if(verletDist>=0) return;
	// automatically initialize from min sphere size; if no spheres, disable stride
	Real minR=Inf;
	for(const shared_ptr<Particle>& p: *dem->particles){
		if(!p || !p->shape) continue;
		Real r=p->shape->equivRadius();
		if(!isnan(r)) minR=min(r,minR);
	}
	for(const shared_ptr<Engine>& e: scene->engines){
		if(!e->isA<Inlet>()) continue;
		Real dMin=e->cast<Inlet>().minMaxDiam()[0];
		// LOG_WARN("{}: dMin={}",e->pyStr(),dMin);
		if(!isnan(dMin)) minR=min(.5*dMin,minR);
	}
	if(!isinf(minR)) verletDist=abs(verletDist)*minR;
	else {
		LOG_WARN("\n  Negative verletDist={} was about to be set from minimum particle radius, but not Particle/Inlet with valid radius was found.\n  SETTING InsertionSortCollider.verletDist=0.0\n  THIS CAN SERIOUSLY DEGRADE PERFORMANCE.\n  Set verletDist=0.0 yourself to get rid of this warning.",verletDist);
		verletDist=0.0;
	};
}


#ifdef WOO_OPENGL
void Gl1_Aabb::go(const shared_ptr<Bound>& bv){
	Aabb& aabb=bv->cast<Aabb>();
	glColor3v(Vector3r(1,1,0));
	if(!scene->isPeriodic){
		glTranslatev(Vector3r(.5*(aabb.min+aabb.max)));
		glScalev(Vector3r(aabb.max-aabb.min));
	} else {
		Vector3r mn=aabb.min, mx=aabb.max;
		// fit infinite bboxes into the cell, if needed
		for(int ax:{0,1,2}){ if(isinf(mn[ax])) mn[ax]=0; if(isinf(mx[ax])) mx[ax]=scene->cell->getHSize().diagonal()[ax]; }
		glTranslatev(Vector3r(scene->cell->shearPt(scene->cell->wrapPt(.5*(mn+mx)))));
		glMultMatrixd(scene->cell->getGlShearTrsfMatrix());
		glScalev(Vector3r(mx-mn));
	}
	glDisable(GL_LINE_SMOOTH);
	glutWireCube(1);
	glEnable(GL_LINE_SMOOTH);
}
#endif
