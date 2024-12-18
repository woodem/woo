#include"Engine.hpp"
#include"Scene.hpp"
#include"Field.hpp"

#include"../supp/pyutil/gil.hpp"

WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR_PY(woo_core_Engine__CLASS_BASE_DOC_ATTRS_CTOR_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_core_ParallelEngine__CLASS_BASE_DOC_ATTRS_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR_PY(woo_core_PeriodicEngine__CLASS_BASE_DOC_ATTRS_CTOR_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_core_PyRunner__CLASS_BASE_DOC_ATTRS);



WOO_PLUGIN(core,(Engine)(ParallelEngine)(PeriodicEngine)(PyRunner));

WOO_IMPL_LOGGER(Engine);

void Engine::run(){ throw std::logic_error((getClassName()+" did not override Engine::run()").c_str()); }

void Engine::explicitRun(const shared_ptr<Scene>& scene_, const shared_ptr<Field>& field_){
	if(!scene_) throw std::runtime_error("Engine.__call__: scene must not be None.");
	scene=scene_.get();
	if(field_){
		// assign field spcified, if possible
		if(!acceptsField(field_.get())) throw std::runtime_error("Engine.__call__: field "+field_->pyStr()+" passed, but is not accepted by the engine.");
		this->field=field_;
	} else {
		// no field given; use one which is acceptable (error if none or more found)
		if(needsField()) setField();
	}
	run();
}

void Engine::setDefaultScene(){ scene=Master::instance().getScene().get(); }

void Engine::setField(){
	if(userAssignedField) return; // do nothing in this case
	if(!needsField()) return; // no field required, do nothing
	vector<shared_ptr<Field> > accepted;
	for(const auto& f: scene->fields){ if(acceptsField(f.get())) accepted.push_back(f); }
	if(accepted.size()>1){
		string err="Engine "+pyStr()+" accepted "+to_string(accepted.size())+" fields to run on:";
		for(const shared_ptr<Field>& f: accepted) err+=" "+f->pyStr();
		err+=". Only one field is allowed; this ambiguity can be resolved by setting the field attribute.";
		throw std::runtime_error(err);
	}
	if(accepted.empty()) throw std::runtime_error("Engine "+pyStr()+" accepted no field to run on; remove it from engines.");
	this->field=accepted[0];
}

void Engine::postLoad(Engine&, void* addr){
	if(addr==NULL){
		if(isNewObject && field){ userAssignedField=true; }
		isNewObject=false;
	}
	if(addr==(void*)&dead){
		notifyDead();
	}
}

shared_ptr<Field> Engine::field_get(){ return field; }

py::object Engine::py_getScene(){
	if(!scene) return py::none();
	else return py::cast(static_pointer_cast<Scene>(scene->shared_from_this()));
}

void Engine::field_set(const shared_ptr<Field>& f){
	if(!f) { setField(); userAssignedField=false; }
	else{ field=f; userAssignedField=true; }
}

void Engine::runPy_generic(const string& callerId, const string& command, Scene* scene_, Engine* engine_, const shared_ptr<Field>& field_){
	if(command.empty()) return;
	GilLock lock;
	try{
		// scripts are run in this namespace (wooMain)
		py::object global(py::import("__main__").attr("__dict__"));
		py::dict local;
		local["scene"]=py::cast(py::ptr(scene_));
		local["S"]=py::cast(py::ptr(scene_));
		local["engine"]=py::cast(py::ptr(engine_));
		local["field"]=py::cast(field_);
		local["woo"]=py::import("woo");
		// local["wooExtra"]=py::import("wooExtra"); // FIXME: not always importable
		py::exec(command.c_str(),global,local);
	} catch (py::error_already_set& e){
		throw std::runtime_error(callerId+": exception in '"+command+"':\n"+parsePythonException_gilLocked(e));
	};
}

void Engine::runPy(const string& callerId, const string& command){
	runPy_generic(callerId,command,scene,this,field);
};


void ParallelEngine::setField(){
	for(vector<shared_ptr<Engine>>& grp: slaves){
		for(const shared_ptr<Engine>& e: grp){
			e->scene=scene;
			e->setField();
		}
	}
}



void ParallelEngine::run(){
	// openMP warns if the iteration variable is unsigned...
	const int size=(int)slaves.size();
	#ifdef WOO_OPENMP
		#pragma omp parallel for
	#endif
	for(int i=0; i<size; i++){
		// run every slave group sequentially
		for(const shared_ptr<Engine>& e: slaves[i]){
			//cerr<<"["<<omp_get_thread_num()<<":"<<e->getClassName()<<"]";
			e->scene=scene;
			if(!e->field && e->needsField()) throw std::runtime_error((getClassName()+" has no field to run on, but requires one.").c_str());
			if(!e->dead && e->isActivated()){ e->run(); }
		}
	}
}

void ParallelEngine::pyHandleCustomCtorArgs(py::args_& args, py::kwargs& kw){
	if(py::len(args)==0) return;
	if(py::len(args)>1) woo::TypeError("ParallelEngine takes 0 or 1 non-keyword arguments ("+to_string(py::len(args))+" given)");
	py::extract<py::list> listEx(args[0]);
	if(!listEx.check()) woo::TypeError("ParallelEngine: non-keyword argument must be a list");
	pySlavesSet(listEx());
	args=py::tuple();
}

void ParallelEngine::pySlavesSet(const py::list& slaves2){
	int len=py::len(slaves2);
	slaves.clear();
	for(int i=0; i<len; i++){
		py::extract<std::vector<shared_ptr<Engine> > > serialGroup(slaves2[i]);
		if (serialGroup.check()){ slaves.push_back(serialGroup()); continue; }
		py::extract<shared_ptr<Engine> > serialAlone(slaves2[i]);
		if (serialAlone.check()){ vector<shared_ptr<Engine> > aloneWrap; aloneWrap.push_back(serialAlone()); slaves.push_back(aloneWrap); continue; }
		woo::TypeError("List elements must be either (a) sequences of engines to be executed one after another (b) individual engines.");
	}
}

py::list ParallelEngine::pySlavesGet(){
	py::list ret;
	for(vector<shared_ptr<Engine>>& grp: slaves){
		if(grp.size()==1) ret.append(py::cast(grp[0]));
		else ret.append(py::cast(grp));
	}
	return ret;
}

void ParallelEngine::getLabeledObjects(const shared_ptr<LabelMapper>& labelMapper){
	for(vector<shared_ptr<Engine> >& grp: slaves) for(const shared_ptr<Engine>& e: grp) Engine::handlePossiblyLabeledObject(e,labelMapper);
	Engine::getLabeledObjects(labelMapper);
};

void PeriodicEngine::fakeRun(){
	const Real& virtNow=scene->time;
	Real realNow=getClock();
	const long& stepNow=scene->step;
	realPrev=realLast; realLast=realNow;
	virtPrev=virtLast; virtLast=virtNow;
	stepPrev=stepLast; stepLast=stepNow;
}

bool PeriodicEngine::isActivated(){
	const Real& virtNow=scene->time;
	Real realNow=getClock();
	const long& stepNow=scene->step;
	// we run for the very first time here, initialize counters
	bool initNow=(stepLast<0);
	if(initNow){
		realLast=realNow; virtLast=virtNow; stepLast=stepNow;
	}
	if(
		(nDo<0 || nDone<nDo) &&
		(	(virtPeriod>0 && virtNow-virtLast>=virtPeriod) ||
		 	(realPeriod>0 && realNow-realLast>=realPeriod) ||
		 	(stepPeriod>0 && ((stepNow-stepLast>=stepPeriod) || (stepModulo && stepNow%stepPeriod==0)))
		)
	){
		// we would run now, but if not desired, don't
		realPrev=realLast; realLast=realNow;
		virtPrev=virtLast; virtLast=virtNow;
		stepPrev=stepLast; stepLast=stepNow;
		// skip the initial run, but have all values of when last run set properly
		if(initNow && !initRun) { return false; }
		nDone++;
		return true;
	}
	return false;
}

void PyRunner::pyHandleCustomCtorArgs(py::args_& t, py::kwargs& d){
	if(py::len(t)==0) return;
	if(py::len(t)==1){
		try {
			command=py::cast<std::string>(t[0]);
			t=py::tuple();
			return;
		} catch(...){ throw std::invalid_argument("Positional argument to PyRunner must be command (a string)."); }
	}
	if(py::len(t)==2){
		try{
			command=py::cast<std::string>(t[0]);
			stepPeriod=py::cast<int>(t[1]);
			t=py::tuple();
			return;
		} catch(...){ };
	}
	if(py::len(t)==2){
		try{
			command=py::cast<std::string>(t[1]);
			stepPeriod=py::cast<int>(t[0]);
			t=py::tuple();
			return;
		} catch(...){ };
	}
	throw std::invalid_argument("Unable to extract positional (unnamed) arguments to PyRunner; must be PyRunner([string]) for command, or PyRunner([string],[int]) or PyRunner([string],[int]) for command and stepPeriod.");
};
