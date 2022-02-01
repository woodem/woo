#include"LabelMapper.hpp"
#include"../supp/pyutil/except.hpp"
#include"Master.hpp"
#include<regex>
#include<boost/algorithm/string.hpp>

WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_core_LabelMapper__CLASS_BASE_DOC_ATTRS_PY);

WOO_PLUGIN(core,(LabelMapper));
WOO_IMPL_LOGGER(LabelMapper);

int LabelMapper::whereIs(const string& label) const {
	if(pyMap.find(label)!=pyMap.end()) return IN_PY;
	if(wooMap.find(label)!=wooMap.end()) return IN_WOO;
	if(wooSeqMap.find(label)!=wooSeqMap.end()) return IN_WOO_SEQ;
	if(modSet.count(label)>0) return IN_MOD;
	return NOWHERE;
}

bool LabelMapper::__contains__(const string& label) const {
	return whereIs(label)!=NOWHERE;
}

py::list LabelMapper::pyKeys() const{
	py::list ret;
	for(const auto& k: this->keys()) ret.append(k);
	return ret;
}

int LabelMapper::__len__() const {
	return wooMap.size()+wooSeqMap.size()+pyMap.size();
}

py::list LabelMapper::pyItems() const{
	py::list ret;
	for(auto& kv: wooMap) ret.append(py::make_tuple(kv.first,kv.second));
	for(auto& kv: wooSeqMap) ret.append(py::make_tuple(kv.first,kv.second));
	for(auto& kv: pyMap) ret.append(py::make_tuple(kv.first,kv.second));
	return ret;
}

void LabelMapper::ensureUsedModsOk(const string& label){
	// no submodules in there
	if(label.find(".")==string::npos) return;
	vector<string> mods; boost::split(mods,label,boost::is_any_of("."));
	string mod;
	// don't check the very last item, that's nod module anymore
	for(size_t i=0; i<mods.size()-1; i++){
		const string& t(mods[i]);
		mod+=(mod.empty()?"":".")+t; // a.b.c
		int where=whereIs(mod);
		if(where==NOWHERE) woo::NameError("Label '"+label+"' requires non-existent pseudo-module '"+mod+"' (use LabelMapper._newModule to create it).");
		if(where!=IN_MOD) woo::NameError("Label '"+label+"' requires pseudo-module '"+mod+"', but this name is already used by another object.");
		// everything ok, do nothing
	}
}

int LabelMapper::labelType(const string& label, string& lab0, int& index) const {
	std::smatch match;
	// catch leading _
	if(std::regex_match(label,match,std::regex("_.*"))) woo::NameError("LabelMapper: labels may not start with underscore (those are reserved to access the underlying LabelMapper object itself).");
	// catch leading dot, two consecutive dots or .[]
	if(std::regex_match(label,match,std::regex("^\\.")) || std::regex_match(label,match,std::regex(".*\\.\\..* ")) || std::regex_match(label,match,std::regex(".*\\.\\[.*"))) woo::NameError("LabelMapper: label '"+label+"' is not a valid python identifier name.");
	// sequence
	if(std::regex_match(label,match,std::regex("([a-zA-Z0-9_\\.]+)\\s*\\[([0-9]+)\\]"))){
		lab0=match[1];
		index=std::stol(match[2]);
		if(index<0) woo::ValueError("LabelMapper: label '"+lab0+"' specifies non-positive index "+to_string(index));
		return LABEL_SEQ;
	}
	// one object
	if(std::regex_match(label,match,std::regex("[a-zA-Z_][.a-zA-Z0-9_]*[a-zA-Z0-9_]*"))){
		return LABEL_PLAIN;
	}
	woo::NameError("LabelMapper: label '"+label+"' is not a valid python identifier name.");
	return -1; // unreachable
}

string LabelMapper::pyAsStr(const py::object& o){
	std::ostringstream oss;
	if(PyObject_HasAttrString(o.ptr(),"__repr__")){
		py::extract<string> ex(o.attr("__repr__")()); // call __repr__
		if(ex.check()) return ex();  // see if the result is a string (should be)
		// if not, proceed to the dummy version below
	}
	oss<<"<"<<py::extract<string>(o.attr("__class__").attr("__name__"))()<<" at "<<o.ptr()<<">";
	return oss.str();
}

template<typename listTuple>
bool LabelMapper::sequence_check_setitem(const string& label, py::object o){
	py::extract<listTuple> exSeq(o);
	if(!exSeq.check()) return false;
	listTuple seq=exSeq();
	int isWoo=-1; // -1 is indeterminate, 0 is pure-python objects, 1 is woo objects
	// this is OK actually
	// if(py::len(seq)==0) woo::ValueError("Sequence given to LabelMapper is empty.");
	for(int i=0; i<py::len(seq); i++){
		// None can be both empty shared_ptr<Object> or None in python, so that one is not decisive
		if(py::object(seq[i]).is_none()) continue; 
		py::extract<shared_ptr<Object>> exi(seq[i]);
		if(exi.check()){
			if(isWoo==0) woo::ValueError("Sequence given to LabelMapper mixes python objects and woo.Object's.");
			isWoo=1;
		} else {
			if(isWoo==1) woo::ValueError("Sequence given to LabelMapper mixes python objects and woo.Object's.");
			isWoo=0;
		}
	}
	switch(isWoo){
		// indeterminate sequence is treated as a sequence of woo::Object
		// it is better that way since assigning python objects will be rejected;
		// the other way, woo::Objects would be assigned as python objects, i.e. not properly pickled and so on
		case -1: {
			// woo::ValueError("Unable to decide whether the sequence contains python objects or woo.Object's (containing only None)."); /*compiler happy*/ return false; 
			vector<shared_ptr<Object>> vec(py::len(seq));
			__setitem__wooSeq(label,vec);
			return true;
		}
		case 0: __setitem__py(label,o); return true;
		case 1: {
			vector<shared_ptr<Object>> vec; vec.reserve(py::len(seq));
			for(int i=0; i<py::len(seq); i++) vec.push_back(py::extract<shared_ptr<Object>>(seq[i])());
			__setitem__wooSeq(label,vec);
			return true;
		}
		// unreachable
		default: throw std::logic_error("LabelMapper::sequence_check_setitem: isWoo=="+to_string(isWoo)+"??");
	}
}


void LabelMapper::opaque_sequence_with_woo_objects_error(py::object oo){
	PyObject* o=oo.ptr();
	if(!PySequence_Check(o)) return; // ok
	if(PySequence_Size(o)<=0) return; // ok
	for(int i=0; i<PySequence_Size(o); i++){
		if(!py::extract<shared_ptr<Object>>(py::cast<py::object>(py::handle(PySequence_GetItem(o,i)))).check()) continue;
		woo::ValueError("Opaque sequence type contains woo.Object's (you have to pass list/tuple so that saving/loading of shared_ptr works properly).");
	}
	return; // probably okay
}

void LabelMapper::__setitem__(const string& label, py::object o){
	py::extract<shared_ptr<Object>> exObj(o);
	py::extract<py::str> exStr(o);
	ensureUsedModsOk(label);
	// empty string (would be mistakenly identified as sequence of other objects
	if(py::isinstance<py::str>(o) || py::isinstance<py::bytes>(o)){ __setitem__py(label,o); return; }
	// woo objects
	if(exObj.check()){ __setitem__woo(label,exObj()); return; }
	// list of woo objects
	if(sequence_check_setitem<py::list>(label,o)) return;
	// tuple of woo objects
	if(sequence_check_setitem<py::tuple>(label,o)) return;
	// some other sequence-like object (e.g. ObjectList) which contains Woo objects (e.g. ObjectList) should be rejected
	opaque_sequence_with_woo_objects_error(o);
	// python object
	__setitem__py(label,o);
}

void LabelMapper::__setitem__woo(const string& label, const shared_ptr<Object>& o){
	string lab0=label; int index; 
	int type=labelType(label,lab0,index);
	int where=whereIs(label);
	if(where==IN_MOD) woo::NameError("Label '"+label+"' is a pseudo-module (cannot be overwritten).");
	bool writable(writables.count(label));
	if(type==LABEL_PLAIN){
		switch(where){
			case IN_WOO:
				assert(wooMap.find(label)!=wooMap.end());
				assert(type==LABEL_PLAIN); // such a label should not have been created at all
				if(wooMap[label].get()!=o.get() && !writable){
					LOG_WARN("Label '{}' overwrites {} with {}",label,wooMap[label]->pyStr(),o->pyStr());
				}
				break;
			case IN_PY:
				assert(pyMap.find(label)!=pyMap.end());
				assert(type==LABEL_PLAIN);
				LOG_WARN("Label '{}' changes type from pure python object to woo.Object, overwriting {} with {}",label,pyAsStr(pyMap[label]),o->pyStr());
				pyMap.erase(label);
				break;
			case IN_WOO_SEQ:
				assert(wooSeqMap.find(label)!=wooSeqMap.end());
				LOG_WARN("Label '{}' changes types from sequence of woo.Object to woo.Object, deleting {} items in the sequence.",label,wooSeqMap.find(label)->second.size());
				wooSeqMap.erase(label);
				break;
			/* NOWEHERE is OK */
		};
		wooMap[label]=o;
	} else {
		assert(type==LABEL_SEQ);
		where=whereIs(lab0); // find base label again
		switch(where){
			case IN_WOO:
				LOG_WARN("Sequence label '{}[{}]' creates a new sequence, deleting '{}' with {}",lab0,index,lab0,wooMap[lab0]->pyStr());
				wooMap.erase(lab0);
				break;
			case IN_PY:
				LOG_WARN("Sequence label '{}[{}]' creates a new sequence, deleting '{}' with {}",lab0,index,lab0,pyAsStr(pyMap[lab0]));
				pyMap.erase(lab0);
				break;
			case NOWHERE: ;
			case IN_WOO_SEQ: ;// ok
		};
		auto& vec=wooSeqMap[lab0];
		if(vec.size()<=(size_t)index) vec.resize(index+1);
		vec[index]=o;
	}
}

void LabelMapper::__setitem__wooSeq(const string& label, const vector<shared_ptr<Object>>& oo){
	string lab0=label; int index; 
	int type=labelType(label,lab0,index);
	if(type==LABEL_SEQ){
		if(oo.empty()){ __setitem__py(label,py::list()); return; } // hack to assign [] properly as python object
		woo::NameError("Subscripted label '"+label+"' may not be assigned a sequence of "+to_string(oo.size())+" woo.core.Objects.");
	}
	int where=whereIs(label);
	if(where==IN_MOD) woo::NameError("Label '"+label+"' is a pseudo-module (cannot be overwritten).");
	bool writable(writables.count(label));
	string old;
	switch(where){
		case IN_WOO:
			old="an existing woo.Object "+wooMap[label]->pyStr();
			wooMap.erase(label);
			break;
		case IN_PY:
			old="an existing pure-python object "+pyAsStr(pyMap[label]);
			pyMap.erase(label);
			break;
		case IN_WOO_SEQ:
			old="an existing sequence with "+to_string(wooSeqMap[label].size())+"items";
			wooSeqMap.erase(label);
			break;
	};
	if(where!=NOWHERE && (!writable || where!=IN_WOO_SEQ)) LOG_WARN("Label '{}' overwrites {} with a new sequence of {} woo.Object's.",label,old,oo.size());
	wooSeqMap[label]=oo;
};

void LabelMapper::__setitem__py(const string& label, py::object o){
	string lab0=label; int index; 
	int type=labelType(label,lab0,index);
	int where=whereIs(label);
	if(where==IN_MOD) woo::NameError("Label '"+label+"' is a pseudo-module (cannot be overwritten).");
	bool writable(writables.count(label));
	assert(!py::extract<shared_ptr<woo::Object>>(o).check());
	if(type==LABEL_PLAIN){
		switch(where){
			case IN_WOO:
				LOG_WARN("Label '{} changes type from woo.Object to pure-python object, overwriting {} with {}",label,wooMap[label]->pyStr(),pyAsStr(o));
				wooMap.erase(label);
				break;
			case IN_PY:
				if(o.ptr()!=pyMap[label].ptr() && !writable) LOG_WARN("Label '{}' overwrites {} with {}",label,pyAsStr(pyMap[label]),pyAsStr(o));
				pyMap.erase(label); // if only replaced later, crashes python (reference counting problem?)
				break;
			case IN_WOO_SEQ:
				LOG_WARN("Label '{}' changes type from sequence of woo.Object to pure-python object, deleting {} items in the sequence.",label,wooSeqMap.find(label)->second.size());
				wooSeqMap.erase(label);
			// NOWHERE is OK
		};
		pyMap[label]=o;
	} else {
		assert(type==LABEL_SEQ);
		where=whereIs(lab0); // find base label again
		switch(where){
			case IN_WOO:
				LOG_WARN("Sequence label '{}[{}]' overwrites '{}' containing {} with a new python list",lab0,index,lab0,wooMap[lab0]->pyStr());
				wooMap.erase(lab0);
				break;
			case IN_WOO_SEQ:
				LOG_WARN("Sequence label '{}[{}]' overwrites '{}' containing {} with a new python list",lab0,index,lab0,wooMap[lab0]->pyStr());
				break;
			case IN_PY:
				if(!py::extract<py::list>(pyMap[lab0]).check()){
					LOG_WARN("Sequence label '{}[{}]' overwrites non-list python object {}",lab0,index,pyAsStr(pyMap[lab0]));
					pyMap[lab0]=py::list();
				} else {
				}
				break;
			case NOWHERE:
				pyMap[lab0]=py::list();
		}
		assert(py::extract<py::list>(pyMap[lab0]).check());
		py::list lst=py::extract<py::list>(pyMap[lab0])();
		// extend the list so that it is long enough
		while(py::len(lst)<index+1) lst.append(py::none());
		if(!py::object(lst[index]).is_none() && !writable) LOG_WARN("Label '{}[{}]' overwrites old item {} with {}",lab0,index,pyAsStr(lst[index]),pyAsStr(o));
		lst[index]=o;
	}
}

py::object LabelMapper::__getitem__(const string& label){
	std::smatch match;
	// hack to be able to do getattr(S.lab,'something[1]')
	if(std::regex_match(label,match,std::regex("^(.*)\\[([0-9]+)\\]$"))){
		string l0=match[1];
		long index=std::stol(match[2]);
		return this->__getitem__(l0).attr("__getitem__")(py::cast(index));
	}
	int where=whereIs(label);
	switch(where){
		// MUST be AttributeError, since hasattr in python tries getattr and catches AttributeError (not NameError)
		case NOWHERE: woo::AttributeError("No such label: '"+label+"'"); break;
		case IN_MOD: woo::ValueError("Label '"+label+"' is a pseudo-module and cannot be obtained directly.");
		case IN_WOO: return py::cast(wooMap[label.c_str()]); break;
		case IN_PY: return pyMap[label]; break;
		case IN_WOO_SEQ: return py::cast(wooSeqMap[label.c_str()]); break; // should convert to list automatically
	};
	abort(); // unreachable
}

void LabelMapper::__delitem__(const string& label){
	int where=whereIs(label);
	switch(where){
		case NOWHERE: woo::NameError("No such label: '"+label+"'"); break;
		case IN_MOD: __delitem__mod(label); break;
		case IN_WOO: wooMap.erase(label); break;
		case IN_PY: pyMap.erase(label); break;
		case IN_WOO_SEQ: wooSeqMap.erase(label); break;
	};
}

void LabelMapper::__delitem__mod(const string& label){
	assert(whereIs(label)==IN_MOD);
	std::list<string> kk=this->keys();
	kk.remove_if([&label](const string& s){ return !boost::starts_with(s,label+"."); });
	for(const auto& k: kk){ if(this->__contains__(k)) this->__delitem__(k); }
	modSet.erase(label);
}


void LabelMapper::setWritable(const string& label, bool writable){
	int where=whereIs(label);
	switch(where){
		case IN_WOO:
		case IN_PY:
		case IN_WOO_SEQ:{
			if(writable) writables.insert(label);
			else writables.erase(label);
			break;
		}
		case IN_MOD:  woo::ValueError("Label '"+label+"' cannot be made writable: is a (pseudo) module.");
		case NOWHERE: woo::NameError("No such label: '"+label+"'");
	}
}

bool LabelMapper::isWritable(const string& label) const{
	if(whereIs(label)==NOWHERE) woo::NameError("No such label: '"+label+"'");
	return writables.count(label)>0;
}

void LabelMapper::newModule(const string& label){
	string mod; vector<string> mm;
	boost::algorithm::split(mm,label,boost::is_any_of("."));
	// mod is "", "foo", "foo.bar", "foo.bar.baz" etc.
	for(size_t i=0; i<mm.size(); i++){
		const auto& m=mm[i];
		mod+=(mod.empty()?"":".")+m;
		int where=whereIs(mod);
		// existing parent module is OK
		if(where==IN_MOD && i<mm.size()-1) continue; 
		// something else having the same name as parent module is not OK
		if(where!=NOWHERE) woo::ValueError("Label '"+mod+"' already exists.");
		// cerr<<"modSet.insert('"<<mod<<"');"<<endl;
		modSet.insert(mod);
	}
}

std::list<string> LabelMapper::keys() const{
	std::list<string> allKeys;
	for(auto& kv: wooMap) allKeys.push_back(kv.first);
	for(auto& kv: wooSeqMap) allKeys.push_back(kv.first);
	for(auto& kv: pyMap) allKeys.push_back(kv.first);
	for(auto& k: modSet) allKeys.push_back(k);
	return allKeys;
}

py::list LabelMapper::__dir__(const string& prefix) const {
	std::list<string> allKeys=this->keys();
	py::list ret;
	if(prefix.empty()){ for(auto& k: allKeys) ret.append(k); }
	else {
		for(auto& k: allKeys){
			if(boost::starts_with(k,prefix)) ret.append(k.substr(prefix.size()));
		}
	}
	return ret;
}
