#if WOO_OPENGL
#include<woo/pkg/gl/GlSetup.hpp>
#include<woo/pkg/gl/Functors.hpp>
#include<woo/pkg/gl/Renderer.hpp>

WOO_PLUGIN(gl,(GlSetup));
WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_gl_GlSetup__CLASS_BASE_DOC_ATTRS_PY);
WOO_IMPL_LOGGER(GlSetup);

vector<std::type_index> GlSetup::objTypeIndices;
vector<string> GlSetup::objTypeNames;
vector<string> GlSetup::objAccessorNames;

void GlSetup::checkIndex(int i) const {
	if(i<0 || i>=(int)objs.size()) woo::IndexError("Invalid index "+to_string(i)+" for GlSetup.objs, must be 0…"+to_string((int)objs.size()-1)+".");
}

string GlSetup::accessorName(const string&s ){
	string ret(s);
	if(boost::starts_with(ret,"Gl1_")) ret=ret.substr(4);
	ret[0]=tolower(ret[0]);
	return ret;
}

void GlSetup::postLoad(GlSetup&,void* attr){
	// the rest is used when loading
	bool ok=true;
	if(objs.size()!=objTypeIndices.size()){ LOG_WARN("GlSetup.objs: incorrect size ("+to_string(objs.size())+", should be "+to_string(objTypeIndices.size())+"), falling back to defaults."); ok=false; }
	if(ok){
		for(size_t i=0; i<objs.size(); i++){
			const auto oPtr=objs[i].get();
			if((oPtr?std::type_index(typeid(*oPtr)):std::type_index(typeid(void)))!=objTypeIndices[i]){
				LOG_WARN("GlSetup.objs["+to_string(i)+"]: incorrect type (is "+(oPtr?objs[i]->pyStr():"None")+", should be a "+objTypeNames[i]+", falling back to defaults.");
				ok=false;
				break;
			}
		}
	}
	if(!ok){
		objs=makeObjs();
		// cerr<<"[GlSetup::postLoad] objs assigned"<<endl;
	}
	dirty=true;
}


py::object GlSetup::pyCallStatic(py::tuple args, py::dict kw){
	if(py::len(kw)>0) woo::RuntimeError("Keyword arguments not accepted.");
	#ifdef WOO_PYBIND11
		py::extract<GlSetup&>(args[0])().pyCall(args.attr("__getitem__")(py::slice(1,-1,1)));
	#else
		py::extract<GlSetup&>(args[0])().pyCall(py::tuple(args.slice(1,py::len(args))));
	#endif
	return py::object();
}

void GlSetup::ensureObjs(){
	if(objs.size()!=objTypeIndices.size()){
		objs=makeObjs();
		// cerr<<"[GlSetup::ensureObjs] objs assigned"<<endl;
	}
}

void GlSetup::pyCall(const py::args_& args){
	// cerr<<"[GlSetup::pyCall]"<<endl;
	ensureObjs();
	for(int i=0; i<py::len(args); i++){
		// objs.resize(objTypeIndices.size());
		py::extract<shared_ptr<Object>> ex(args[i]);
		if(!ex.check()) woo::TypeError("Only instances of woo.core.Object can be given as arguments.");
		const auto o(ex()); const auto oPtr=o.get();
		if(!o) woo::TypeError("None not accepted as argument.");
		auto I=std::find(objTypeIndices.begin(),objTypeIndices.end(),std::type_index(typeid(*oPtr)));
		if(I==objTypeIndices.end()) woo::TypeError(o->pyStr()+" is of type not understood by GlSetup.");
		objs[I-objTypeIndices.begin()]=o;
		dirty=true;
	}
}


void GlSetup::pyHandleCustomCtorArgs(py::args_& args, py::kwargs& kw){
	// cerr<<"[GlSetup::pyHandleCustomArgs]"<<endl;
	pyCall(args);
	args=py::tuple();
	ensureObjs();
	// not sure this is really useful
	#ifdef WOO_PYBIND11
	for(auto item: kw){
		string key=py::cast<string>(item.first);
	#else
	py::list kwl=kw.items();
	for(int i=0; i<py::len(kwl); i++){
		py::tuple item=py::extract<py::tuple>(kwl[i]);
		string key=py::extract<string>(item[0]);
	#endif
		auto I=std::find(objAccessorNames.begin(),objAccessorNames.end(),key);
		// we don't consume unknown keys, Object's business to use them or error out
		if(I==objAccessorNames.end()) continue; 
		size_t index=I-objAccessorNames.begin();
		#ifdef WOO_PYBIND11
			if(!py::isinstance<shared_ptr<Object>>(item.second)) woo::TypeError(key+" must be a "+objTypeNames[index]+".");
			auto o=py::cast<shared_ptr<Object>>(item.second);
		#else
			py::extract<shared_ptr<Object>> ex(item[1]);
			if(!ex.check()) woo::TypeError(key+" must be a "+objTypeNames[index]+".");
			const auto o(ex());
		#endif
		const auto oPtr=o.get();
		if(!o) woo::TypeError(key+" must not be None.");
		if(std::type_index(typeid(*oPtr))!=objTypeIndices[index]) woo::TypeError(key+" must be a "+objTypeNames[index]+" (not "+o->getClassName()+").");
		objs[index]=o;
		py::api::delitem(kw,key.c_str());
	}
}

py::list GlSetup::getObjNames() const{
	py::list ret;
	for(const auto& s: objAccessorNames) ret.append(s);
	return ret;
}


shared_ptr<Renderer> GlSetup::getRenderer() {
	ensureObjs();
	if(objs.empty()) throw std::runtime_error("GlSetup::getRenderer: objs.empty()");
	if(!objs[0]->isA<Renderer>()) throw std::runtime_error("GlSetup::getRenderer: !objs[0]->isA<Renderer>()");
	return static_pointer_cast<Renderer>(objs[0]);
}

vector<shared_ptr<Object>> GlSetup::makeObjs() {
	// cerr<<"[GlSetup::makeObjs]"<<endl;
	// functors for dispatcher types
	std::list<shared_ptr<Object>> field,shape,bound,node,cPhys;
	#define _PROBE_FUNCTOR(functorT,list,className) if(Master::instance().isInheritingFrom_recursive(className,#functorT)){ list.push_back(Master::instance().factorClass(className)); }
	for(auto& item: Master::instance().getClassBases()){
		_PROBE_FUNCTOR(GlFieldFunctor,field,item.first);
		_PROBE_FUNCTOR(GlShapeFunctor,shape,item.first);
		_PROBE_FUNCTOR(GlBoundFunctor,bound,item.first);
		_PROBE_FUNCTOR(GlNodeFunctor,node,item.first);
		_PROBE_FUNCTOR(GlCPhysFunctor,cPhys,item.first);
	}
	#undef _PROBE_FUNCTOR
	vector<shared_ptr<Object>> ret;
	ret.push_back(make_shared<Renderer>());
	for(auto l: {&field,&shape,&bound,&node,&cPhys}){
		if(l->empty()) continue;
		ret.push_back(shared_ptr<Object>()); // separator
		l->sort([](const shared_ptr<Object>& a, const shared_ptr<Object>& b){ return a->getClassName()<b->getClassName(); });
		for(const auto& o: *l) ret.push_back(o);
	}
	#if 0
	for(size_t i=0; i<ret.size(); i++){ cerr<<"GlSetup::defaultObjs: "<<i<<": "<<(ret[i]?ret[i]->getClassName():string("-"))<<endl; }
	#endif
	return ret;
}


#endif /* WOO_OPENGL */
