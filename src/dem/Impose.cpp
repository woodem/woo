#include"Impose.hpp"
#include"../supp/smoothing/LinearInterpolate.hpp"
#include"Contact.hpp"

WOO_PLUGIN(dem,(HarmonicOscillation)(AlignedHarmonicOscillations)(CircularOrbit)(StableCircularOrbit)(ConstantForce)(RadialForce)(Local6Dofs)(VariableAlignedRotation)(InterpolatedMotion)(VelocityAndReadForce)(ReadForce)(CombinedImpose)(VariableVelocity3d));

WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_HarmonicOscillation__CLASS_BASE_DOC_ATTRS_CTOR);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_CircularOrbit__CLASS_BASE_DOC_ATTRS_CTOR);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_AlignedHarmonicOscillations__CLASS_BASE_DOC_ATTRS_CTOR);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_ConstantForce__CLASS_BASE_DOC_ATTRS_CTOR);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_RadialForce__CLASS_BASE_DOC_ATTRS_CTOR);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_Local6Dofs__CLASS_BASE_DOC_ATTRS_CTOR);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_VariableAlignedRotation__CLASS_BASE_DOC_ATTRS_CTOR);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_InterpolatedMotion__CLASS_BASE_DOC_ATTRS_CTOR);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_StableCircularOrbit__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_VelocityAndReadForce__CLASS_BASE_DOC_ATTRS_CTOR);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_ReadForce__CLASS_BASE_DOC_ATTRS_CTOR);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_VariableVelocity3d__CLASS_BASE_DOC_ATTRS_CTOR);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_CombinedImpose__CLASS_BASE_DOC_ATTRS);


void CircularOrbit::velocity(const Scene* scene, const shared_ptr<Node>& n){
	if(!node) throw std::runtime_error("CircularOrbit: node must not be None.");
	Vector3r& vv(n->getData<DemData>().vel);
	Vector2r p2((node->ori.conjugate()*(n->pos-node->pos)).head<2>()); // local 2d position
	Vector2r v2((node->ori.conjugate()*vv).head<2>()); // local 2d velocity
	// in polar coords
	Real theta=atan2(p2[1],p2[0]);
	Real rho=p2.norm();
	// tangential velocity at this point
	Real v2t=omega*rho; // XXX: might be actually smaller due to leapfrog?
	Real ttheta=theta+.5*v2t*scene->dt; // mid-step polar angle
	vv=node->ori*(Quaternionr(AngleAxisr(ttheta,Vector3r::UnitZ()))*Vector3r(0,v2t,0));
	if(rotate) n->getData<DemData>().angVel=node->ori*Vector3r(0,0,omega);
	if(isFirstStepRun(scene)) angle+=omega*scene->dt;
}

void StableCircularOrbit::velocity(const Scene* scene, const shared_ptr<Node>& n){
	if(!node) throw std::runtime_error("StableCircularOrbit: node must not be None.");
	const auto& pos0(n->pos);
	Vector3r rtz0(CompUtils::cart2cyl(node->glob2loc(n->pos))); // curr pos in local cyl (r, theta, z)
	Vector3r rtz1(radius,rtz0[1]+omega*scene->dt,rtz0[2]);    // target pos in local cyl
	Vector3r pos1(node->loc2glob(CompUtils::cyl2cart(rtz1))); // target pos in global cart
	n->getData<DemData>().vel=(pos1-pos0)/scene->dt;
	if(rotate) n->getData<DemData>().angVel=node->ori*Vector3r(0,0,omega);
	if(isFirstStepRun(scene)) angle+=omega*scene->dt;
}

void ConstantForce::force(const Scene* scene, const shared_ptr<Node>& n){
	n->getData<DemData>().force+=F;
}

void RadialForce::force(const Scene* scene, const shared_ptr<Node>& n){
	if(!nodeA || !nodeB) throw std::runtime_error("RadialForce: nodeA and nodeB must not be None.");
	const Vector3r& A(nodeA->pos);
	const Vector3r& B(nodeB->pos);
	Vector3r axis=(B-A).normalized();
	Vector3r C=A+axis*((n->pos-A).dot(axis)); // closest point on the axis
	Vector3r towards=C-n->pos;
	if(towards.squaredNorm()==0) return; // on the axis, do nothing
	n->getData<DemData>().force-=towards.normalized()*F;
}

void Local6Dofs::selfTest(const shared_ptr<Node>& n, const shared_ptr<DemData>& dyn, const string& prefix) const {
	for(int i=3; i<6; i++){
		if(whats[i]!=0 && dyn->useAsphericalLeapfrog()){ throw std::runtime_error(prefix+": refusing to prescribe rotational DoF (axis="+to_string(i-3)+") since the node is integrated aspherically."); }
	}
}


// function to impose both, distinguished by the last argument
void Local6Dofs::doImpose(const Scene* scene, const shared_ptr<Node>& n, bool velocity){
	DemData& dyn=n->getData<DemData>();
	for(int i=0;i<6;i++){
		if(velocity&&(whats[i]&Impose::VELOCITY)){
			if(i<3){
				//dyn.vel+=ori.conjugate()*(Vector3r::Unit(i)*(values[i]-(ori*dyn.vel).dot(Vector3r::Unit(i))));
				Vector3r locVel=ori*dyn.vel; locVel[i]=values[i]; dyn.vel=ori.conjugate()*locVel;
			} else {
				//dyn.angVel+=ori.conjugate()*(Vector3r::Unit(i%3)*(values[i]-(ori*dyn.angVel).dot(Vector3r::Unit(i%3))));
				Vector3r locAngVel=ori*dyn.angVel; locAngVel[i%3]=values[i]; dyn.angVel=ori.conjugate()*locAngVel;
			}
		}
		if(!velocity&&(whats[i]&Impose::FORCE)){
			// set absolute value, cancel anything set previously (such as gravity)
			if(i<3){ dyn.force =ori.conjugate()*(values[i]*Vector3r::Unit(i)); }
			else   { dyn.torque=ori.conjugate()*(values[i]*Vector3r::Unit(i%3)); }
		}
	}
}

void VariableAlignedRotation::selfTest(const shared_ptr<Node>& n, const shared_ptr<DemData>& dyn, const string& prefix) const {
	if(dyn->useAsphericalLeapfrog()){ throw std::runtime_error(prefix+": refusing to prescribe rotations since the node is integrated aspherically."); }
}

void VariableAlignedRotation::postLoad(VariableAlignedRotation&,void*){
	if(timeAngVel.size()>1){
		for(size_t i=0; i<timeAngVel.size()-1; i++){
			if(!(timeAngVel[i+1].x()>=timeAngVel[i].x())) woo::ValueError("VariableAlignedRotation.timeAngVel: time values must be non-decreasing ("+to_string(timeAngVel[i].x())+".."+to_string(timeAngVel[i+1].x())+" at "+to_string(i)+".."+to_string(i+1));
		}
	}
}

void VariableAlignedRotation::velocity(const Scene* scene, const shared_ptr<Node>& n){
	auto& dyn=n->getData<DemData>();
	Real t=scene->time;
	if(wrap && t>timeAngVel.back().x()) t=std::fmod(t,timeAngVel.back().x());
	dyn.angVel[axis]=linearInterpolate(t,timeAngVel,_interpPos);
}

void InterpolatedMotion::selfTest(const shared_ptr<Node>& n, const shared_ptr<DemData>& dyn, const string& prefix) const {
	if(dyn->useAsphericalLeapfrog()){ throw std::runtime_error(prefix+": refusing to prescribe rotations since the node is integrated aspherically."); }
}

void InterpolatedMotion::postLoad(InterpolatedMotion&,void* attr){
	if(attr!=NULL) return; // just some value being set
	if(poss.size()!=oris.size() || oris.size()!=times.size()) throw std::runtime_error("InterpolatedMotion: poss, oris, times must have the same length (not "+to_string(poss.size())+", "+to_string(oris.size())+", "+to_string(times.size()));
}

void InterpolatedMotion::velocity(const Scene* scene, const shared_ptr<Node>& n){
	Real nextT=scene->time+scene->dt-t0;
	Vector3r nextPos=linearInterpolate(nextT,times,poss,_interpPos);
	Quaternionr nextOri, oriA, oriB; Real relAB;
	std::tie(oriA,oriB,relAB)=linearInterpolateRel(nextT,times,oris,_interpPos);
	nextOri=oriA.slerp(relAB,oriB);
	DemData& dyn=n->getData<DemData>();
	dyn.vel=(nextPos-n->pos)/scene->dt;
	// Leapfrog::leapfrogSphericalRotate says q⁺=rq⁰, where r is quaternion
	// by saying r=q⁺q⁰*, we have q⁺=rq⁰=q⁺q⁰*q⁰≡q⁺
	// (r is transported as rotation vector, but converted from quaternion here and to quaternion in Leapfrog)
	AngleAxisr rot(nextOri*n->ori.conjugate());
	dyn.angVel=rot.axis()*rot.angle()/scene->dt;
	// very verbose logging
	#if 0
		auto qrep=[](Quaternionr& q){ AngleAxisr aa(q); return "("+to_string(aa.axis()[0])+" "+to_string(aa.axis()[1])+" "+to_string(aa.axis()[2])+"|"+to_string(aa.angle())+")"; };
		cerr<<"InterpolatedMotion:"<<endl<<
		"  oriA="<<qrep(oriA)<<", oriB="<<qrep(oriB)<<", relAB="<<relAB<<endl<<
		"  n->ori: curr="<<qrep(n->ori)<<", next="<<qrep(nextOri)<<endl<<
		"  rot=("<<rot.axis().transpose()<<"|"<<rot.angle()<<"), angVel="<<dyn.angVel.transpose()<<endl;
	#endif
};

void ReadForce::readForce(const Scene* scene, const shared_ptr<Node>& n){
	{
		std::scoped_lock l(lock);
		// first comes, first resets for this step
		if(stepLast!=scene->step){ stepLast=scene->step; F.reset(); T.reset(); }
	}
	const auto& nf=n->getData<DemData>().force;
	const auto& nt=n->getData<DemData>().torque;
	if(!node){
		F+=nf;
		T+=nt;
	} else {
		F+=node->ori.conjugate()*nf; // only rotate
		T+=node->ori.conjugate()*(nt+(n->pos-node->pos).cross(nf));
	}
}



void VelocityAndReadForce::velocity(const Scene* scene, const shared_ptr<Node>& n){
	auto& dyn=n->getData<DemData>();
	if(latBlock) dyn.vel=dir*vel;
	else{
		Vector3r vLat=dyn.vel-dir*(dyn.vel.dot(dir));
		dyn.vel=vLat+dir*vel;
	}
}

void VelocityAndReadForce::readForce(const Scene* scene, const shared_ptr<Node>& n){
	{
		std::scoped_lock l(lock);
		// first comes, first resets for this step
		if(stepLast!=scene->step){
			stepLast=scene->step;
			sumF.reset();
			dist+=vel*scene->dt;
		}
	}
	// the rest is thread-safe
	Real f=n->getData<DemData>().force.dot(dir);
	sumF+=(invF?-1:1)*f;
	if(scene->trackEnergy && !energyName.empty()) scene->energy->add(f*scene->dt*vel,energyName,workIx,EnergyTracker::IsIncrement | EnergyTracker::ZeroDontCreate);
}

void VariableVelocity3d::postLoad(VariableVelocity3d&,void*){
	if(times.size()!=vels.size()) throw std::runtime_error("VariableVelocity3d: times and vels must have the same length (not "+to_string(times.size())+", "+to_string(vels.size())+").");
	if(!angVels.empty() && times.size()!=angVels.size()) throw std::runtime_error("VariableVelocity3d.angVels: if not empty, it length ("+to_string(angVels.size())+" must be equal to the length of times ("+to_string(times.size())+").");
	for(int i=0; i<(int)times.size()-1; i++){
		if(times[i]>=times[i+1]) woo::ValueError("VariableVelocity3d.times: must be non-decreasing (times["+to_string(i)+"]="+to_string(times[i])+", times["+to_string(i+1)+"]="+to_string(times[i+1])+").");
	}
	if(wrap && !times.empty() && times[0]!=0) woo::ValueError("VariableVelocity3d.times: when wrap is True, the first time value must be zero (not "+to_string(times[0])+").");
}

void VariableVelocity3d::selfTest(const shared_ptr<Node>& n, const shared_ptr<DemData>& dyn, const string& prefix) const {
	if(!angVels.empty()){
		if(dyn->useAsphericalLeapfrog()){
			throw std::runtime_error(prefix+": refusing to prescribe rotations since the node is integrated aspherically.");
		}
	}
}


WOO_IMPL_LOGGER(VariableVelocity3d);

void VariableVelocity3d::velocity(const Scene* scene, const shared_ptr<Node>& n){
	if(times.empty()) throw std::runtime_error("VariableVelocity3d: times must not be empty.");
	bool rot=(ori!=Quaternionr::Identity());
	DemData& dyn(n->getData<DemData>());
	// current velocity
	Vector3r v=rot?(ori.conjugate()*dyn.vel):dyn.vel;
	// current time (+dt/2 because of leapfrog), minus phase
	Real t=scene->time+.5*scene->dt-t0;
	// wrap if wanted and needed
	if(wrap && (t<0 || t>times.back())) t=CompUtils::wrapNum(t,times.back());
	// _interpPosVel could be accessed for both read/write by several threads potentially disturbing the lookup process
	// use local copy and write it back
	Vector3r vImp=linearInterpolate(t,times,vels,_interpPosVel);
	/*
		with diff==true, use the current velocity as basis, otherwise use 0
		if the imposed component is NaN, then keep current value
		these two conditions combined yield somewhat convolved expression:
		diff==true : v[ax]+0
		diff==false: 0+v[ax]
	*/
	for(short ax:{0,1,2}) v[ax]=(diff?v[ax]:0)+(isnan(vImp[ax])?(diff?0:v[ax]):vImp[ax]);
	dyn.vel=rot?ori*v:v;
	// angular velocity
	if(!angVels.empty()){
		// if(dyn.useAsphericalLeapfrog()) throw std::runtime_error("VariableVelocity3d.angVels: imposing angular velocity on node "+n->pyStr()+" is not supported as it uses aspherical leapfrog integration routine based on angular momentum.");
		Vector3r av=rot?(ori.conjugate()*dyn.angVel):dyn.angVel;
		Vector3r avImp=linearInterpolate(t,times,angVels,_interpPosAngVel);
		// dtto as above
		for(short ax:{0,1,2}) av[ax]=(diff?av[ax]:0)+(isnan(avImp[ax])?(diff?0:av[ax]):avImp[ax]);
		dyn.angVel=rot?ori*av:av;
	}
	if(hooks.empty()) return;
	Real prevTimeOrig;
	if(isFirstStepRun(scene,&prevTimeOrig)){
		// transform prevTime back to normalized time (fitting *times* points)
		Real prevTimeNormalized=prevTimeOrig+.5*scene->dt-t0;
		int cycle=0;
		if(wrap && prevTimeNormalized>times.back()){
			prevTimeNormalized=CompUtils::wrapNum(prevTimeNormalized,times.back(),cycle);
		}
		// find hook point which we just passed
		int hookNo=-1;
		if(wrap & (prevTimeNormalized>t+2*scene->dt)){ // we just wrapped around
			hookNo=0;
		} else {
			for(size_t i=0; i<times.size(); i++){
				if(prevTimeNormalized<times[i] && times[i]<=t){ // take away the part for leapfrog correction above
					hookNo=i;
					break;
				}
			}
		}
		if(hookNo>=0 && !hooks[hookNo].empty()){
			py::gil_scoped_acquire lock;
			try{
					py::object global(py::module_::import_("wooMain").attr("__dict__"));
					py::dict local;
					local["scene"]=py::cast(py::ptr(scene));
					local["S"]=py::cast(py::ptr(scene));
					local["woo"]=py::module_::import_("woo");
					local["node"]=py::cast(n);
					local["self"]=py::cast(this->shared_from_this());
					local["timeIndex"]=hookNo;
					local["cycle"]=cycle;
					local["prevTime"]=prevTimeOrig;
					#ifdef WOO_NANOBIND
						local.update(global.cast<py::dict>());
						py::exec(hooks[hookNo].c_str(),local);
					#else
						py::exec(hooks[hookNo].c_str(),global,local);
					#endif
			} catch (py::error_already_set& e){
					LOG_ERROR("Python error from {}.hook[{}]: {}",this->pyStr(),hookNo,hooks[hookNo]);
					LOG_ERROR("Node {}, time {}. Error follows.",n->pyStr(),scene->time);
					LOG_ERROR("{}",parsePythonException_gilLocked(e));
			}
		}
	}
}


void CombinedImpose::selfTest(const shared_ptr<Node>& n, const shared_ptr<DemData>& dyn, const string& prefix) const {
	for(size_t i=0; i<imps.size(); i++){
		if(!imps[i]) throw std::runtime_error(prefix+".imps["+to_string(i)+"]==None (must be an Impose instance).");
		imps[i]->selfTest(n,dyn,prefix+".imps["+to_string(i)+"]");
	}
}



void CombinedImpose::postLoad(CombinedImpose&,void*){
	what=Impose::NONE;
	for(const auto& i: imps) what|=i->what;
}
void CombinedImpose::velocity(const Scene* scene, const shared_ptr<Node>& n){
	for(auto& i: imps){
		if(i->what & Impose::VELOCITY) i->velocity(scene,n);
	}
}
void CombinedImpose::force(const Scene* scene, const shared_ptr<Node>& n){
	for(auto& i: imps){
		if(i->what & Impose::FORCE) i->force(scene,n);
	}
}
void CombinedImpose::readForce(const Scene* scene, const shared_ptr<Node>& n){
	for(auto& i: imps){
		if(i->what & Impose::READ_FORCE) i->readForce(scene,n);
	}
}

// !! declared in pkg/dem/Particle.hpp
shared_ptr<Impose> Impose::pyAdd(const shared_ptr<Impose>& b) {
	auto c=make_shared<CombinedImpose>();
	if(this->isA<CombinedImpose>()) c->imps=this->cast<CombinedImpose>().imps;
	else c->imps.push_back(static_pointer_cast<Impose>(shared_from_this()));
	if(b->isA<CombinedImpose>()){ const auto& b2=b->cast<CombinedImpose>(); c->imps.insert(c->imps.end(),b2.imps.begin(),b2.imps.end()); }
	else c->imps.push_back(b);
	// updates what
	c->postLoad(*c,/*not important*/&(c->imps));
	return c;
}
