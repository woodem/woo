#include"LawTester.hpp"
#include"../supp/pyutil/gil.hpp"
#include"L6Geom.hpp"
#include"FrictMat.hpp"
#include"Sphere.hpp"

WOO_PLUGIN(dem,(LawTesterStage)(LawTester));

WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_LawTesterStage__CLASS_BASE_DOC_ATTRS_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_LawTester__CLASS_BASE_DOC_ATTRS_PY);

WOO_IMPL_LOGGER(LawTesterStage);
WOO_IMPL_LOGGER(LawTester);

void LawTesterStage::pyHandleCustomCtorArgs(py::args_& args, py::kwargs& kw){
	// go through the dict, find just values we need
	if(!kw.contains("whats")) return;
	string whatStr;
	try {
		whatStr=std::string(py::str(kw["whats"]));
	} catch(...){
		return;
	}
	//py::extract<string> isStr(py::cast<py::object>(kw["whats"]));
	//if(!isStr.check()) return;
	// string whatStr=isStr();
	kw.attr("pop")("whats"); // remove from kw
	if(whatStr.size()!=6) woo::ValueError("LawTesterStage.whats, if given as string, must have length 6, not "+to_string(whatStr.size())+".");
	for(int i=0;i<6;i++){
		char w=whatStr[i];
		if(w!='f' && w!='v' && w!='.' && w!='i') woo::ValueError("LawTesterStage.whats["+to_string(i)+"]: must be 'f' (force) or 'v' (velocity) or 'i' (initial velocity) or '.' (nothing prescribed). not '"+w+"'.");
		switch(w){
			case '.': whats[i]=Impose::NONE; break;
			case 'v': whats[i]=Impose::VELOCITY; break;
			case 'f': whats[i]=Impose::FORCE; break;
			case 'i': whats[i]=Impose::INIT_VELOCITY; break;
			default: LOG_FATAL("?!?"); abort();
		}
	}
};

void LawTesterStage::reset(){
	step=0;
	time=0;
	hadC=hasC=false;
	bounces=0;
	timeC0=NaN;
}


void LawTester::restart(){
	f=k=u=v=Vector6r::Zero();
	stage=0;
	stageT0=-1;
	for(const auto& s: stages) s->reset();
	// in case this has been changed by the user
	dead=false; /*useless, but consistent*/notifyDead();
}

void LawTester::run(){
	const auto& dem=static_pointer_cast<DemField>(field);
	if(ids[0]<0 || ids[0]>=(long)dem->particles->size() || !(*dem->particles)[ids[0]] || ids[1]<0 || ids[1]>=(long)dem->particles->size() || !(*dem->particles)[ids[1]]) throw std::runtime_error("LawTester.ids=("+to_string(ids[0])+","+to_string(ids[1])+"): invalid particle ids.");
	const shared_ptr<Particle>& pA=(*dem->particles)[ids[0]];
	const shared_ptr<Particle>& pB=(*dem->particles)[ids[1]];
	if(!pA->shape || pA->shape->nodes.size()!=1 || !pB->shape || pB->shape->nodes.size()!=1) throw std::runtime_error("LawTester: only mono-nodal particles are handled now.");
	// get radii of spheres, but don't fail if they are not spheres
	// radius is only needed when bending is prescribed
	// and we only fail later if that is the case
	Sphere *s1=dynamic_cast<Sphere*>(pA->shape.get()), *s2=dynamic_cast<Sphere*>(pA->shape.get());
	Real rads[]={NaN,NaN};
	if(s1) rads[0]=s1->radius;
	if(s2) rads[1]=s2->radius;
	Node* nodes[2]={pA->shape->nodes[0].get(),pB->shape->nodes[0].get()};
	DemData* dyns[2]={nodes[0]->getDataPtr<DemData>().get(),nodes[1]->getDataPtr<DemData>().get()};

	for(auto dyn: dyns){
		if(!dyn->impose) dyn->impose=make_shared<Local6Dofs>();
		else if(!dyn->impose->isA<Local6Dofs>()) throw std::runtime_error("LawTester: DemData.impose is set, but it is not a Local6Dofs.");
	}
	Local6Dofs* imposes[2]={static_cast<Local6Dofs*>(dyns[0]->impose.get()),static_cast<Local6Dofs*>(dyns[1]->impose.get())};

	shared_ptr<Contact> C=dem->contacts->find(ids[0],ids[1]);
	// this is null ptr if there is no contact, or the contact is only potential
	if(C && !C->isReal()) C=shared_ptr<Contact>();
	shared_ptr<L6Geom> l6g;
	if(C){
		l6g=dynamic_pointer_cast<L6Geom>(C->geom);
		if(!l6g) throw std::runtime_error("LawTester: C.geom is not a L6Geom");
	}

	// compute "contact" orientation even if there is no contact (in that case, only normal things will be prescribed)
	Quaternionr ori;
	if(!C){
		Vector3r locX=(nodes[1]->pos-nodes[0]->pos).normalized();
		Vector3r locY=locX.cross(abs(locX[1])<abs(locX[2])?Vector3r::UnitY():Vector3r::UnitZ()); locY-=locX*locY.dot(locX); locY.normalize(); Vector3r locZ=locX.cross(locY);Matrix3r T; T.row(0)=locX; T.row(1)=locY; T.row(2)=locZ;
		ori=Quaternionr(T);
	} else {
		ori=C->geom->node->ori;
	}

	if(stage+1>(int)stages.size()) throw std::runtime_error("LawTester.stage out of range (0…"+to_string(((int)stages.size())-1));
	const auto& stg=stages[stage];
	/* we are in this stage for the first time */
	if(stageT0<0){
		stg->step=0;
		stg->time=0;
		stageT0=scene->time;
		stg->hadC=stg->hasC=false;
		/* prescribe values and whats */
		for(int i:{0,1}){
			int sign=(i==0?-1:1);
			Real weight=(i==0?1-abWeight:abWeight);
			// DemData* dyn(dyns[i]);
			Local6Dofs* impose(imposes[i]);
			impose->ori=ori;
			// make copy which is modified
			for(int ix=0;ix<6;ix++){
				switch(stg->whats[ix]){
					case Impose::FORCE:{
						// apply force to one particle only (0 or one, whichever is closer to weight)
						// and block corresponding velocity component of the other particle
						bool useForceHere=((i==0&&abWeight<.5) || (i==1 && abWeight>=.5));
						if(useForceHere){
							// when imposing force, check that it will be used by the integrator
							if(!dyns[i]->isBlockedNone()){
								throw std::runtime_error("LawTester: when imposing force, particle may not have *any* DoFs blocked via DemData.blocked; blocking is handled by imposing velocities/forces through LawTester directly (blocked DoFs: #"+to_string(ids[0])+": '"+dyns[0]->blocked_vec_get()+"', #"+to_string(ids[1])+": '"+dyns[1]->blocked_vec_get()+"')");
							}
							impose->whats[ix]=Impose::FORCE;
							impose->values[ix]=sign*stg->values[ix];
						} else {
							impose->whats[ix]=Impose::VELOCITY;
							impose->values[ix]=0.;
						}
						break;
					}
					// initial velocity is set just like velocity, but reset in the else branch when the stage is used next time
					case Impose::INIT_VELOCITY:
					case Impose::VELOCITY:{
						Real w=weight;
						// shear is distributed antisymmetrically, so we reset the sign here
						//if(ix==1 || ix==2) w*=sign;
						if(ix==4 || ix==5){
							// bending must be distributed according to radii, as to not induce shear
							w=(1-rads[i]/(rads[0]+rads[1]));
							if(!(rads[0]>0 && rads[1]>0)) throw std::runtime_error("LawTester: when bending is prescribed for particles, both must be spheres. It is not yet implemented for one sphere only (that is perfectly possible); it is geometrically not possible for two non-spherical shapes.");
						}
						impose->whats[ix]=Impose::VELOCITY;
						impose->values[ix]=w*sign*stg->values[ix];
						break;
					}
					case Impose::NONE:{
						impose->whats[ix]=Impose::NONE;
						impose->values[ix]=0.;
						break;
					}
					default:
						LOG_FATAL("?!?"); abort();
				}
			}
			// compensate shear, which is perpendicular to the current normal, not mid-step normal
			// that would lead to normal extension, which is not desirable
			// if normal force is prescribed, it will better take care of itself, without compensations here
			if(stg->whats[0]==Impose::VELOCITY){
				if(stg->whats[1]==Impose::VELOCITY){
					// TODO
				}
			}
		}
	} else {
		/* only update stage values */
		stg->step++;
		stg->time=scene->time-stageT0;
		for(int i:{0,1}){
			// update local coords
			imposes[i]->ori=ori;
			// check where only initial velocity was prescribed and remove that imposition
			for(int ix=0; ix<6; ix++){
				if(stg->whats[ix]!=Impose::INIT_VELOCITY) continue;
				imposes[i]->whats[ix]=Impose::NONE;
				imposes[i]->values[ix]=0.;
			}
		}
	};

	/* save smooth data */
	if(smoothErr<0) smoothErr=smooth;
	if(!C){
		f=v=u=k=smooF=smooV=smooU=Vector6r::Constant(NaN);
		fErrRel=fErrAbs=uErrRel=uErrAbs=vErrRel=vErrAbs=Vector6r::Constant(NaN);
		stg->hasC=false;
	} else {
		// keep track of v[0] changing sign
		// 0s and NaNs will not pass here, which is eaxctly what we want
		if(stg->step>0 && v[0]*l6g->vel[0]<0){
			LOG_DEBUG("Incrementing bounces, as v[0] changes sign: prev {}, curr {}.",v[0],l6g->vel[0]);
			stg->bounces+=1;
		}
		if(C->isFresh(scene)) stg->timeC0=scene->time-stageT0;
		f<<C->phys->force,C->phys->torque;
		v<<l6g->vel,l6g->angVel;
		if(dynamic_pointer_cast<FrictPhys>(C->phys)){
			const FrictPhys& ph=C->phys->cast<FrictPhys>();
			k<<Vector3r(ph.kn,ph.kt,ph.kt),Vector3r::Zero();
		} else k=Vector6r::Constant(NaN);
		stg->hasC=stg->hadC=true;
		if(isnan(smooF[0])){ // the contact is new in this step (or we are new), just save unsmoothed value
			u=Vector6r::Zero(); u[0]=l6g->uN;
			smooF=f; smooV=v; smooU=u;
			fErrRel=fErrAbs=uErrRel=uErrAbs=vErrRel=vErrAbs=Vector6r::Constant(NaN);
		} else {
			u+=v*scene->dt; // cumulative in u
			u[0]=l6g->uN;
			// smooth all values here
			#define _SMOO_ERR(foo,smooFoo,fooErrRel,fooErrAbs) smooFoo=(1-smooth)*smooFoo+smooth*foo; {\
				Vector6r rErr=((foo.array()-smooFoo.array())/smooFoo.array()).abs().matrix(); Vector6r aErr=(foo.array()-smooFoo.array()).abs().matrix();\
				if(isnan(fooErrRel[0])){ fooErrRel=rErr; fooErrAbs=aErr; } \
				else{ fooErrRel=(1-smoothErr)*fooErrRel+smoothErr*rErr; fooErrAbs=(1-smoothErr)*fooErrAbs+smoothErr*aErr; } \
			}
			_SMOO_ERR(f,smooF,fErrRel,fErrAbs);
			_SMOO_ERR(u,smooU,uErrRel,uErrAbs);
			_SMOO_ERR(v,smooV,vErrRel,vErrAbs);
			#undef _SMOO_ERR
		}
	}

	if(stg->untilEvery>1 && (stg->step%stg->untilEvery)>0) return;

	GilLock lock; // lock the interpreter for this block
	string* errCmd=nullptr; // to know where the error happened (traceback does not show that)
	/* check the result of stg->until */
	try{
		py::object main=py::import("__main__");
		py::object globals=main.attr("__dict__");
		py::dict locals;
		locals["C"]=C?py::cast(C):py::none();
		locals["pA"]=py::cast(pA);
		locals["pB"]=py::cast(pB);
		locals["stage"]=py::cast(stg);
		locals["scene"]=py::cast(scene->shared_from_this());
		locals["S"]=py::cast(scene->shared_from_this());
		// this will give a nice error message when energy is not used
		locals["E"]=scene->trackEnergy?py::cast(scene->energy):py::none();
		locals["tester"]=py::cast(this->shared_from_this());

		errCmd=&stg->until;
		py::object result=py::eval(stg->until.c_str(),globals,locals);
		errCmd=nullptr;

		bool isDone=py::extract<bool>(result)();
		if(isDone){
			LOG_INFO("Stage {} done at step {}",stage,scene->step);
			stageT0=-1;
			if(!stg->done.empty()){
				errCmd=&stg->done;
				py::exec(stg->done.c_str(),globals,locals);
				errCmd=nullptr;
			}
			if(stage<(int)stages.size()-1) stage++;
			else{ // finished
				py::exec(done.c_str(),globals,locals);
			};
			/* ... */
		}
	} catch (py::error_already_set& e){
		throw std::runtime_error("LawTester exception"+(errCmd?(" in '"+*errCmd+"'"):string(" "))+":\n"+parsePythonException_gilLocked(e));
	}
};
