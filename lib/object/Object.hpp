#pragma once

#include<woo/lib/base/Types.hpp>

#include<boost/any.hpp>
#include<boost/version.hpp>
#include<boost/type_traits.hpp>
#include<boost/preprocessor.hpp>
#include<boost/type_traits/integral_constant.hpp>
#include<woo/lib/pyutil/doc_opts.hpp>
#include<woo/lib/pyutil/except.hpp>
#include<woo/lib/pyutil/pickle.hpp>
#include<woo/lib/pyutil/compat.hpp>

#include<boost/preprocessor.hpp>
#include<boost/version.hpp>

#include<woo/lib/object/serialization.hpp>
#include<woo/lib/object/ObjectIO.hpp>

#include<woo/lib/base/Math.hpp>
#include<woo/lib/base/Logging.hpp>
#include<woo/lib/object/AttrTrait.hpp>



/*! Macro defining what classes can be found in this plugin -- must always be used in the respective .cpp file.
 * A function registerThisPluginClasses_FirstPluginName is generated at every occurence. The identifier should
 * be unique and avoids use of __COUNTER__ which didn't appear in gcc until 4.3.
 */
#if BOOST_VERSION<104200
 	#error Boost >= 1.42 is required
#endif

#ifdef WOO_CEREAL
	/* .hpp */
	#define WOO_REGISTER_OBJECT(name)  CEREAL_FORCE_DYNAMIC_INIT(name);
	/* .cpp */
	#define _WOO_PLUGIN_BOOST_REGISTER(x,y,z) CEREAL_REGISTER_TYPE(z); CEREAL_REGISTER_DYNAMIC_INIT(z);
#else
	#define _WOO_PLUGIN_BOOST_REGISTER(x,y,z) BOOST_CLASS_EXPORT_IMPLEMENT(z); BOOST_SERIALIZATION_FACTORY_0(z);
	#define WOO_REGISTER_OBJECT(name) BOOST_CLASS_EXPORT_KEY(name);
#endif

#define _PLUGIN_CHECK_REPEAT(x,y,z) void z::must_use_both_WOO_CLASS_BASE_DOC_ATTRS_and_WOO_PLUGIN(){}
#define _WOO_PLUGIN_REPEAT(x,y,z) BOOST_PP_STRINGIZE(z),
#define _WOO_FACTORY_REPEAT(x,y,z) __attribute__((unused)) bool BOOST_PP_CAT(_registered,z)=Master::instance().registerClassFactory(BOOST_PP_STRINGIZE(z),(Master::FactoryFunc)([](void)->shared_ptr<woo::Object>{ return make_shared<z>(); }));
// priority 500 is greater than priority for log4cxx initialization (in core/main/pyboot.cpp); therefore lo5cxx will be initialized before plugins are registered
#define WOO__ATTRIBUTE__CONSTRUCTOR __attribute__((constructor))

#define WOO_PLUGIN(module,plugins) BOOST_PP_SEQ_FOR_EACH(_WOO_FACTORY_REPEAT,~,plugins); namespace{ WOO__ATTRIBUTE__CONSTRUCTOR void BOOST_PP_CAT(registerThisPluginClasses_,BOOST_PP_SEQ_HEAD(plugins)) (void){ LOG_DEBUG_EARLY("Registering classes in "<<__FILE__); const char* info[]={__FILE__ , BOOST_PP_SEQ_FOR_EACH(_WOO_PLUGIN_REPEAT,~,plugins) NULL}; Master::instance().registerPluginClasses(BOOST_PP_STRINGIZE(module),info);} } BOOST_PP_SEQ_FOR_EACH(_WOO_PLUGIN_BOOST_REGISTER,~,plugins) BOOST_PP_SEQ_FOR_EACH(_PLUGIN_CHECK_REPEAT,~,plugins)




// empty functions for ADL
//namespace{
	template<typename T>	void preLoad(T&){}; template<typename T> void postLoad(T& obj, void* addr){ /* cerr<<"Generic no-op postLoad("<<typeid(T).name()<<"&) called for "<<obj.getClassName()<<std::endl; */ }
	template<typename T>	void preSave(T&){}; template<typename T> void postSave(T&){}
//};

using namespace woo;

// see:
//		https://bugs.launchpad.net/woo/+bug/539562
// 	http://www.boost.org/doc/libs/1_42_0/libs/python/doc/v2/faq.html#topythonconversionfailed
// for reason why the original def_readwrite will not work:
// #define _PYATTR_DEF(x,thisClass,z) .def_readwrite(BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2,0,z)),&thisClass::BOOST_PP_TUPLE_ELEM(2,0,z),BOOST_PP_TUPLE_ELEM(2,1,z))
#define _PYATTR_DEF(x,thisClass,z) _DEF_READWRITE_CUSTOM(thisClass,z)
//
// return reference for vector and matrix types to allow things like
// O.bodies.pos[1].state.vel[2]=0 
// returning value would only change copy of velocity, without propagating back to the original
//
// see http://www.mail-archive.com/woo-dev@lists.launchpad.net/msg03406.html
//
// note that for sequences (like vector<> etc), values are returned; but in case of 
// vector of shared_ptr's, things inside are still shared, so
// O.engines[2].gravity=(0,0,9.33) will work
//
// OTOH got sequences of non-shared types, it sill (silently) fail:
// f=Facet(); f.vertices[1][0]=4 
//
// see http://www.boost.org/doc/libs/1_42_0/libs/type_traits/doc/html/boost_typetraits/background.html
// about how this works
namespace woo{
	// by default, do not return reference; return value instead
	template<typename T> struct py_wrap_ref: public boost::false_type{};
	// specialize for types that should be returned as references
	template<> struct py_wrap_ref<Vector3r>: public boost::true_type{};
	template<> struct py_wrap_ref<Vector3i>: public boost::true_type{};
	template<> struct py_wrap_ref<Vector2r>: public boost::true_type{};
	template<> struct py_wrap_ref<Vector2i>: public boost::true_type{};
	template<> struct py_wrap_ref<Quaternionr>: public boost::true_type{};
	template<> struct py_wrap_ref<Matrix3r>: public boost::true_type{};
	template<> struct py_wrap_ref<MatrixXr>: public boost::true_type{};
	template<> struct py_wrap_ref<Matrix6r>: public boost::true_type{};
	template<> struct py_wrap_ref<Vector6r>: public boost::true_type{};
	template<> struct py_wrap_ref<Vector6i>: public boost::true_type{};
	template<> struct py_wrap_ref<VectorXr>: public boost::true_type{};

	//template<class C, typename T, T C::*A>
	//void make_setter_postLoad(C& instance, const T& val){ instance.*A=val; cerr<<"make_setter_postLoad called"<<endl; postLoad(instance); }
};

// ADL only works within the same namespace
// this duplicate is for classes that are not in woo:: namespace (yet)
template<class C, typename T, T C::*A>
void make_setter_postLoad(C& instance, const T& val){ instance.*A=val; /* cerr<<"make_setter_postLoad called"<<endl; */ instance.callPostLoad((void*)&(instance.*A)); /* postLoad(instance,(void*)&(instance.*A)); */ }

template<bool integral> struct _bit_accessors_if_integral;
// do-nothing variant
template<> struct _bit_accessors_if_integral<false> {
	template<typename classObjT, typename classT, typename attrT, attrT classT::*A>
	static void doRegister(classObjT& _classObj, const vector<string>& bits, bool ro){ };
	template<typename IntegralT>
	static bool trySetNamedBit(const string& name, bool ro, const vector<string>& bits, IntegralT& flags, const py::object& v){ return false; }
};
// register bits variant
template<> struct _bit_accessors_if_integral<true> {
	template<typename classObjT, typename classT, typename attrT, attrT classT::*A>
	static void doRegister(classObjT& _classObj, const vector<string>& bits, bool ro){
		for(size_t i=0; i<bits.size(); i++){
			auto getter=[i](const classT& obj){ return bool(obj.*A & (1<<i)); };
			auto setter=[i](classT& obj, bool val){ if(val) obj.*A|=(1<<i); else obj.*A&=~(1<<i); };
			if(ro) _classObj.add_property_readonly(bits[i].c_str(),getter);
			else   _classObj.add_property(bits[i].c_str(),getter,setter);
		}
	}
	template<typename IntegralT>
	static bool trySetNamedBit(const string& name, bool ro, const vector<string>& bits, IntegralT& flags, const py::object& val){
		if(bits.empty()) return false;
		for(size_t i=0; i<bits.size(); i++){
			if(name!=bits[i]) continue;
			if(ro) throw std::runtime_error("Flags accessed via bit accessor "+name+" are read-only.");
			py::extract<bool> ex(val);
			if(!ex.check()) throw std::runtime_error("Failed bool conversion when setting named bit "+name+".");
			if(ex()) flags|=(1<<i);
			else flags&=~(1<<i);
			return true;
		}
		return false;
	}
};

// define accessors raising InvalidArugment at every access
template<typename classObjT, typename traitT>
void _wooDef_deprecatedProperty(classObjT& _classObj, traitT& trait){
	_classObj.add_property(trait._name.c_str(),
		[trait](py::object self)->void{ throw std::invalid_argument("Error accessing "+trait._className+"."+trait._name+": "+trait._doc); },
		[trait](py::object self, py::object val){ throw std::invalid_argument("Error accessing "+trait._className+"."+trait._name+": "+trait._doc); },
		trait._doc.c_str())
	;
};

#pragma GCC visibility push(hidden)
template<bool namedEnum> struct  _def_woo_attr__namedEnum{};
/* instantiation for attribute which IS NOT not a named enumeration */
template<> struct _def_woo_attr__namedEnum<false>{
	template<typename classObjT, typename traitT, typename classT, typename attrT, attrT classT::*A>
	void wooDef(classObjT& _classObj, traitT& trait, const char* className, const char *attrName){
		if(trait.isDeprecated()){ _wooDef_deprecatedProperty(_classObj,trait); return; }
		bool _ro=trait.isReadonly(), _post=trait.isTriggerPostLoad(), _ref(!_ro && (woo::py_wrap_ref<attrT>::value || trait.isPyByRef()));
		const char* docStr(trait._doc.c_str());
		if      ( _ref && !_ro && !_post) _classObj.def_readwrite(attrName,A,docStr);
		else if ( _ref && !_ro &&  _post) _classObj.add_property(attrName,[](const classT& o){ return o.*A; },make_setter_postLoad<classT,attrT,A>,docStr);
		else if ( _ref &&  _ro)           _classObj.def_readonly(attrName,A,docStr);
		else if (!_ref && !_ro && !_post) _classObj.add_property(attrName,[](classT& o){ return o.*A; },[](classT& o, const attrT& val){ o.*A=val; },py::return_value_policy::copy,docStr);
		else if (!_ref && !_ro &&  _post) _classObj.add_property(attrName,[](classT& o){ return o.*A; },make_setter_postLoad<classT,attrT,A>,py::return_value_policy::copy,docStr);
		else if (!_ref &&  _ro)           _classObj.add_property_readonly(attrName,[](const classT& o){ return o.*A; },py::return_value_policy::copy,docStr);
		if(_ro && _post) cerr<<"WARN: "<<className<<"::"<<attrName<<" with the woo::Attr::readonly flag also uselessly sets woo::Attr::triggerPostLoad."<<endl;
		if(!trait._bits.empty()) _bit_accessors_if_integral<std::is_integral<attrT>::value>::template doRegister<classObjT,classT,attrT,A>(_classObj,trait._bits,_ro && (!trait._bitsRw));
	}
};

/* instantiation for attribute which IS a named enumeration */
// FIXME: this works but does not look pretty
// see the comment of http://stackoverflow.com/a/25281985/761090
// https://mail.python.org/pipermail/cplusplus-sig/2009-February/014263.html documents the current solution
template<> struct _def_woo_attr__namedEnum<true>{
	template<typename classObjT, typename traitT, typename classT, typename attrT, attrT classT::*A>
	void wooDef(classObjT& _classObj, traitT& trait, const char* className, const char *attrName) 
	{
		if(trait.isDeprecated()){ _wooDef_deprecatedProperty(_classObj,trait); return; }
		bool _ro=trait.isReadonly(), _post=trait.isTriggerPostLoad();
		const char* docStr(trait._doc.c_str());
		auto getter=[trait](const classT& obj){ return trait.namedEnum_num2name(obj.*A); };
		auto setter=[trait](classT& obj, py::object val){ obj.*A=trait.namedEnum_name2num(val);};
		auto setterPostLoad=[trait](classT& obj, py::object val){ obj.*A=trait.namedEnum_name2num(val); obj.callPostLoad((void*)&(obj.*A)); };
		if (_ro)                _classObj.add_property_readonly(attrName,getter,docStr);
		else if(!_ro && !_post) _classObj.add_property(attrName,getter,setter,docStr);
		else if(!_ro &&  _post) _classObj.add_property(attrName,getter,setterPostLoad,docStr);
	}
};
#pragma GCC visibility pop

#define _DEF_READWRITE_CUSTOM(thisClass,attr) if(!(_ATTR_TRAIT(thisClass,attr).isHidden())){ auto _trait(_ATTR_TRAIT(thisClass,attr)); constexpr bool isNamedEnum(!!(_ATTR_TRAIT_TYPE(thisClass,attr)::compileFlags & woo::Attr::namedEnum)); _def_woo_attr__namedEnum<isNamedEnum>().wooDef<decltype(_classObj),_ATTR_TRAIT_TYPE(thisClass,attr),thisClass,decltype(thisClass::_ATTR_NAM(attr)),&thisClass::_ATTR_NAM(attr)>(_classObj, _trait, BOOST_PP_STRINGIZE(thisClass), _ATTR_NAM_STR(attr)); }



// print warning about deprecated attribute; thisClass is type name, not string
#define _DEPREC_ERROR(thisClass,deprec) throw std::invalid_argument(BOOST_PP_STRINGIZE(thisClass) "." BOOST_PP_STRINGIZE(_DEPREC_OLDNAME(deprec)) " is no longer supported: " _DEPREC_COMMENT(deprec));
/* kw attribute setter */
#define _PYSET_ATTR_DEPREC(x,thisClass,z) if(key==BOOST_PP_STRINGIZE(_DEPREC_OLDNAME(z))){ _DEPREC_ERROR(thisClass,z); }
/* expose exception-raising accessors to python */
#define _PYATTR_DEPREC_DEF(x,thisClass,z) .add_property(BOOST_PP_STRINGIZE(_DEPREC_OLDNAME(z)),&thisClass::BOOST_PP_CAT(_getDeprec_,_DEPREC_OLDNAME(z)),&thisClass::BOOST_PP_CAT(_setDeprec_,_DEPREC_OLDNAME(z)),"Deprecated attribute, raises exception when accessed:" _DEPREC_COMMENT(z))
#define _PYHASKEY_ATTR_DEPREC(x,thisClass,z) if(key==BOOST_PP_STRINGIZE(_DEPREC_OLDNAME(z))) return false;
/* accessors functions raising error */
#define _ACCESS_DEPREC(x,thisClass,z) /*getter*/ int BOOST_PP_CAT(_getDeprec_,_DEPREC_OLDNAME(z))(){ _DEPREC_ERROR(thisClass,z); /*compiler happy*/return -1; } /*setter*/ void BOOST_PP_CAT(_setDeprec_,_DEPREC_OLDNAME(z))(const py::object&){_DEPREC_ERROR(thisClass,z); }


// static switch to make hidden attributes not settable via ctor args in python
// this avoids compile-time error with boost::multi_array which with py::extract
template<bool hidden, bool namedEnum> struct _setAttrMaybe{};
template<bool namedEnum> struct _setAttrMaybe</*hidden*/true,namedEnum>{
	template<typename traitT, typename Tsrc, typename Tdst>
	static void set(traitT& trait, const string& name, const Tsrc& src, Tdst& dst){ woo::AttributeError(name+" is not settable from python (marked as hidden)."); }
};
template<> struct _setAttrMaybe</*hidden*/false,/*namedEnum*/false>{
	template<typename traitT, typename Tsrc, typename Tdst>
	static void set(traitT& trait, const string& name, const Tsrc& src, Tdst& dst){ dst=py::extract<Tdst>(src); }
};
template<> struct _setAttrMaybe</*hidden*/false,/*namedEnum*/true>{
	template<typename traitT, typename Tsrc, typename Tdst>
	static void set(traitT& trait, const string& name, const Tsrc& src, Tdst& dst){ dst=trait.namedEnum_name2num(src); }
};

template<typename T,typename std::enable_if<std::is_base_of<py::handle,T>::value>::type* =nullptr> py::object _asPyObject(const T& o){ return py::object(o); }
template<typename T,typename std::enable_if<!std::is_base_of<py::handle,T>::value>::type* =nullptr> py::object _asPyObject(const T& o){ return py::cast(o); }

// loop bodies for attribute access
// _PYGET_ATTR is unused
// #define _PYGET_ATTR(x,y,z) if(key==_ATTR_NAM_STR(z)) return py::object(_ATTR_NAM(z));
#define _PYSET_ATTR(x,klass,z) { typedef _ATTR_TRAIT_TYPE(klass,z) traitT; if(key==_ATTR_NAM_STR(z)) { _setAttrMaybe<!!(traitT::compileFlags & woo::Attr::hidden),!!(traitT::compileFlags & woo::Attr::namedEnum)>::set(_ATTR_TRAIT_GET(klass,z)(),key,value,_ATTR_NAM(z)); return; } if(_bit_accessors_if_integral<std::is_integral<_ATTR_TYP(z)>::value>::template trySetNamedBit(key,(traitT::compileFlags&woo::Attr::readonly),_ATTR_TRAIT(klass,z)._bits,_ATTR_NAM(z),value)) return; }

#define _PYHASKEY_ATTR(x,y,z) if(key==_ATTR_NAM_STR(z)) return true;
#define _PYATTR_TRAIT(x,klass,z)        traitList.append(py::cast(static_cast<AttrTraitBase*>(&_ATTR_TRAIT_GET(klass,z)())));
#define _PYDICT_ATTR(x,y,z) if(!(_ATTR_TRAIT(klass,z).isHidden()) && (all || !(_ATTR_TRAIT(klass,z).isNoSave())) && (all || !(_ATTR_TRAIT(klass,z).isNoDump()))){ /*if(_ATTR_TRAIT(klass,z) & woo::Attr::pyByRef) ret[_ATTR_NAM_STR(z)]=py::object(boost::ref(_ATTR_NAM(z))); else */  ret[_ATTR_NAM_STR(z)]=_asPyObject(_ATTR_NAM(z)); }


// static switch for noSave attributes via templates
template<bool noSave> struct _SerializeMaybe{};
template<> struct _SerializeMaybe<true>{
	template<class ArchiveT, typename T>
	static void serialize(ArchiveT& ar, T& obj, const char* name){
		/*std::cerr<<"["<<name<<"]";*/
		ar & cereal::make_nvp(name,obj);
	}
};
template<> struct _SerializeMaybe<false>{
	template<class ArchiveT, typename T>
	static void serialize(ArchiveT& ar, T&, const char* name){ /* do nothing */ }
};


// serialization of a single attribute
#define _WOO_BOOST_SERIALIZE_REPEAT(x,klass,z) _SerializeMaybe<!(_ATTR_TRAIT_TYPE(klass,z)::compileFlags & woo::Attr::noSave)>::serialize(ar,_ATTR_NAM(z), BOOST_PP_STRINGIZE(_ATTR_NAM(z)));

// TODO: handle this by defining boost::serialization::base_class alias for base_object
#ifdef WOO_CEREAL
	#define _WOO_SERIALIZE_BASE_NVP(baseClass) cereal::make_nvp(#baseClass,cereal::base_class<baseClass>(this))
#else
	#define _WOO_SERIALIZE_BASE_NVP(baseClass) cereal::make_nvp(#baseClass,cereal::base_object<baseClass>(*this))
#endif
// the body of the serialization function
#define _WOO_BOOST_SERIALIZE_BODY(thisClass,baseClass,attrs) \
	ar & _WOO_SERIALIZE_BASE_NVP(baseClass);  \
	/* with ADL, either the generic (empty) version above or baseClass::preLoad etc will be called (compile-time resolution) */ \
	if(archive_is_loading<ArchiveT>()) preLoad(*this); else preSave(*this); \
	BOOST_PP_SEQ_FOR_EACH(_WOO_BOOST_SERIALIZE_REPEAT,thisClass,attrs) \
	if(archive_is_loading<ArchiveT>()) postLoad(*this,NULL); else postSave(*this);

// declaration/implementation version of the whole serialization function
// declaration first:
#define _WOO_BOOST_SERIALIZE_DECL(thisClass,baseClass,attrs)\
	friend class cereal::access;\
	private: template<class ArchiveT> void serialize(ArchiveT & ar,  std::uint32_t const version);

#ifdef WOO_CEREAL
	/* same for cereal? maybe, must be tested */
	#define _WOO_BOOST_SERIALIZE_IMPL_INSTANTIATE(x,thisClass,archiveType) template void thisClass::serialize<archiveType>(archiveType & ar, unsigned int version);
	#define _WOO_BOOST_SERIALIZE_IMPL(thisClass,baseClass,attrs)\
		template<class ArchiveT> void thisClass::serialize(ArchiveT & ar, std::uint32_t const version){ \
			_WOO_BOOST_SERIALIZE_BODY(thisClass,baseClass,attrs) \
		} \
		/* explicit instantiation for all avialbale archive types; not sure if necessary for cereal */ \
		BOOST_PP_SEQ_FOR_EACH(_WOO_BOOST_SERIALIZE_IMPL_INSTANTIATE,thisClass,WOO_BOOST_ARCHIVES)
#else
	#define _WOO_BOOST_SERIALIZE_IMPL_INSTANTIATE(x,thisClass,archiveType) template void thisClass::serialize<archiveType>(archiveType & ar, unsigned int version);
	#define _WOO_BOOST_SERIALIZE_IMPL(thisClass,baseClass,attrs)\
		template<class ArchiveT> void thisClass::serialize(ArchiveT & ar, std::uint32_t const version){ \
			_WOO_BOOST_SERIALIZE_BODY(thisClass,baseClass,attrs) \
		} \
		/* explicit instantiation for all available archive types -- see http://www.boost.org/doc/libs/1_55_0/libs/serialization/doc/pimpl.html */ \
		BOOST_PP_SEQ_FOR_EACH(_WOO_BOOST_SERIALIZE_IMPL_INSTANTIATE,thisClass,WOO_BOOST_ARCHIVES)
#endif

// inline version of the serialization function
// no need for explcit instantiation, as the code is in headers
#define _WOO_BOOST_SERIALIZE_INLINE(thisClass,baseClass,attrs) \
	friend class cereal::access; \
	private: template<class ArchiveT> void serialize(ArchiveT & ar, std::uint32_t const version){ \
		_WOO_BOOST_SERIALIZE_BODY(thisClass,baseClass,attrs) \
	}



#define _REGISTER_ATTRIBUTES_DEPREC(thisClass,baseClass,attrs,deprec) \
	_WOO_BOOST_SERIALIZE_INLINE(thisClass,baseClass,attrs) public: \
	void pySetAttr(const std::string& key, const py::object& value) override {BOOST_PP_SEQ_FOR_EACH(_PYSET_ATTR,thisClass,attrs); BOOST_PP_SEQ_FOR_EACH(_PYSET_ATTR_DEPREC,thisClass,deprec); baseClass::pySetAttr(key,value); } \
	/* return dictionary of all acttributes and values; deprecated attributes omitted */ py::dict pyDict(bool all=true) const override { py::dict ret; BOOST_PP_SEQ_FOR_EACH(_PYDICT_ATTR,thisClass,attrs); WOO_PY_DICT_UPDATE(baseClass::pyDict(all),ret); return ret; } \
	void callPostLoad(void* addr) override { baseClass::callPostLoad(addr); postLoad(*this,addr); }



// getters for individual fields
#define _ATTR_TYP(s) BOOST_PP_TUPLE_ELEM(5,0,s)
#define _ATTR_NAM(s) BOOST_PP_TUPLE_ELEM(5,1,s)
#define _ATTR_INI(s) BOOST_PP_TUPLE_ELEM(5,2,s)
#define _ATTR_TRAIT(klass,s) makeAttrTrait(BOOST_PP_TUPLE_ELEM(5,3,s)).doc(_ATTR_DOC(s)).className(BOOST_PP_STRINGIZE(klass)).name(_ATTR_NAM_STR(s)).cxxType(_ATTR_TYP_STR(s)).ini(_ATTR_TYP(s)(_ATTR_INI(s)))
// trait type name within the _Traits container
#define _ATTR_TRAIT_TYPE__INTERNAL(s) BOOST_PP_CAT(_ATTR_NAM(s),_T)
// trait type witin the klass itself
#define _ATTR_TRAIT_TYPE(klass,s) _ATTR_TRAITS_STRUCT_NAME(klass)::_ATTR_TRAIT_TYPE__INTERNAL(s)
#define _ATTR_TRAIT_GET(klass,s) _ATTR_TRAITS_STRUCT_NAME(klass)::_ATTR_NAM(s)
#define _ATTR_DOC(s) BOOST_PP_TUPLE_ELEM(5,4,s)
// stringized getters
#define _ATTR_TYP_STR(s) BOOST_PP_STRINGIZE(_ATTR_TYP(s))
#define _ATTR_NAM_STR(s) BOOST_PP_STRINGIZE(_ATTR_NAM(s))
#define _ATTR_INI_STR(s) BOOST_PP_STRINGIZE(_ATTR_INI(s))

// each class klass contains struct klass::klass_Traits;
// it is usually only incompletely declared in the header and defined in the implementation file
#define _ATTR_TRAITS_STRUCT_NAME(klass) BOOST_PP_CAT(klass,_Traits)
#define _ATTR_TRAITS_CONTAINER_DECL(klass) struct _ATTR_TRAITS_STRUCT_NAME(klass)
#define _ATTR_TRAITS_CONTAINER_IMPL(klass,attrs) struct klass::_ATTR_TRAITS_STRUCT_NAME(klass){ BOOST_PP_SEQ_FOR_EACH(_WOO_TRAIT_DECL,klass,attrs) };
#define _ATTR_TRAITS_CONTAINER_HEADERONLY(klass,attrs) struct _ATTR_TRAITS_STRUCT_NAME(klass){ BOOST_PP_SEQ_FOR_EACH(_WOO_TRAIT_DECL,klass,attrs) };

// deprecated specification getters
#define _DEPREC_OLDNAME(x) BOOST_PP_TUPLE_ELEM(2,0,x)
#define _DEPREC_COMMENT(x) BOOST_PP_TUPLE_ELEM(2,1,x)

#define _PY_REGISTER_CLASS_BODY(thisClass,baseClass,classTrait,attrs,deprec,extras) \
	checkPyClassRegistersItself(#thisClass); \
	WOO_SET_DOCSTRING_OPTS; \
	auto traitPtr=make_shared<ClassTrait>(classTrait); traitPtr->name(#thisClass).file(__FILE__).line(__LINE__); \
	py::class_<thisClass,shared_ptr<thisClass>,baseClass> _classObj(mod,#thisClass,traitPtr->getDoc().c_str()); \
	return [traitPtr,_classObj,mod]() mutable { \
		_classObj.def(py::init<>([](){ shared_ptr<thisClass> instance=make_shared<thisClass>(); instance->callPostLoad(NULL); return instance; })); \
		_classObj.def(py::init([](py::args& a, py::kwargs& k){ return Object_ctor_kwAttrs<thisClass>(a,k);})); \
		_classObj.def(py::pickle([](const shared_ptr<thisClass>& self){ return self->pyDict(/*all*/false); },&Object__setstate__<thisClass>)); \
		_classObj.attr("_classTrait")=traitPtr; \
		BOOST_PP_SEQ_FOR_EACH(_PYATTR_DEF,thisClass,attrs); \
		(void) _classObj BOOST_PP_SEQ_FOR_EACH(_PYATTR_DEPREC_DEF,thisClass,deprec); \
		(void) _classObj extras ; \
		py::list traitList; BOOST_PP_SEQ_FOR_EACH(_PYATTR_TRAIT,thisClass,attrs); _classObj.attr("_attrTraits")=traitList;\
		Object::derivedCxxClasses.push_back(py::object(_classObj)); \
	}

#define _WOO_CLASS_BASE_DOC_ATTRS_DEPREC_PY(thisClass,baseClass,classTrait,attrs,deprec,extras) \
	_REGISTER_ATTRIBUTES_DEPREC(thisClass,baseClass,attrs,deprec) \
	REGISTER_CLASS_AND_BASE(thisClass,baseClass) \
	/* accessors for deprecated attributes, with warnings */ BOOST_PP_SEQ_FOR_EACH(_ACCESS_DEPREC,thisClass,deprec) \
	/* python class registration */ std::function<void()> pyRegisterClass(py::module_& mod) override { _PY_REGISTER_CLASS_BODY(thisClass,baseClass,classTrait,attrs,deprec,extras); } \
	void must_use_both_WOO_CLASS_BASE_DOC_ATTRS_and_WOO_PLUGIN(); // virtual ensures v-table for all classes 

// attribute declaration
#define _WOO_ATTR_DECL(x,thisClass,z) _ATTR_TYP(z) _ATTR_NAM(z);
// trait declaration (inside the Traits struct body)
#define _WOO_TRAIT_DECL(x,thisClass,z) \
	static auto& _ATTR_NAM(z)(){ static auto _tmp=_ATTR_TRAIT(thisClass,z); return _tmp; } \
	typedef std::remove_reference<decltype(_ATTR_TRAIT(thisClass,z))>::type _ATTR_TRAIT_TYPE__INTERNAL(z);

// return "type name;", define trait type and getter
// #define _ATTR_DECL_AND_TRAIT(x,thisClass,z) _WOO_ATTR_DECL(x,thisClass,z) _WOO_TRAIT_DECL(x,thisClass,z)


// return name(default), (for initializers list); TRICKY: last one must not have the comma
#define _ATTR_MAKE_INITIALIZER(x,maxIndex,i,z) BOOST_PP_TUPLE_ELEM(2,0,z)(BOOST_PP_TUPLE_ELEM(2,1,z)) BOOST_PP_COMMA_IF(BOOST_PP_NOT_EQUAL(maxIndex,i))
#define _ATTR_NAME_ADD_DUMMY_FIELDS(x,y,z) ((/*type*/,z,/*default*/,/*flags*/,/*doc*/))
#define _ATTR_MAKE_INIT_TUPLE(x,y,z) (( _ATTR_NAM(z),_ATTR_INI(z) ))


/********************** USER MACROS START HERE ********************/

// attrs is (type,name,init-value,docstring)
#define WOO_CLASS_BASE_DOC(klass,base,doc)                             WOO_CLASS_BASE_DOC_ATTRS_INIT_CTOR_PY(klass,base,doc,,,,)
#define WOO_CLASS_BASE_DOC_ATTRS(klass,base,doc,attrs)                 WOO_CLASS_BASE_DOC_ATTRS_INIT_CTOR_PY(klass,base,doc,attrs,,,)
#define WOO_CLASS_BASE_DOC_ATTRS_CTOR(klass,base,doc,attrs,ctor)       WOO_CLASS_BASE_DOC_ATTRS_INIT_CTOR_PY(klass,base,doc,attrs,,ctor,)
#define WOO_CLASS_BASE_DOC_ATTRS_PY(klass,base,doc,attrs,py)           WOO_CLASS_BASE_DOC_ATTRS_INIT_CTOR_PY(klass,base,doc,attrs,,,py)
#define WOO_CLASS_BASE_DOC_ATTRS_CTOR_PY(klass,base,doc,attrs,ctor,py) WOO_CLASS_BASE_DOC_ATTRS_INIT_CTOR_PY(klass,base,doc,attrs,,ctor,py)
#define WOO_CLASS_BASE_DOC_ATTRS_CTOR_DTOR_PY(klass,base,doc,attrs,ctor,dtor,py) WOO_CLASS_BASE_DOC_ATTRS_INIT_CTOR_DTOR_PY(klass,base,doc,attrs,,ctor,dtor,py)
#define WOO_CLASS_BASE_DOC_ATTRS_INIT_CTOR_DTOR_PY(klass,base,doc,attrs,inits,ctor,dtor,py) WOO_CLASS_BASE_DOC_ATTRS_DEPREC_INIT_CTOR_DTOR_PY(klass,base,doc,attrs,,inits,ctor,dtor,py)

// some shortcuts
#define WOO_CLASS_BASE_DOC_ATTRS_INIT_PY(klass,base,doc,attrs,inits,py) WOO_CLASS_BASE_DOC_ATTRS_INIT_CTOR_DTOR_PY(klass,base,doc,attrs,inits,,,py)
#define WOO_CLASS_BASE_DOC_ATTRS_INIT_CTOR_PY(klass,base,doc,attrs,inits,ctor,py) WOO_CLASS_BASE_DOC_ATTRS_INIT_CTOR_DTOR_PY(klass,base,doc,attrs,inits,ctor,,py)

// the most general
#define WOO_CLASS_BASE_DOC_ATTRS_DEPREC_INIT_CTOR_DTOR_PY(thisClass,baseClass,classTraitSpec,attrs,deprec,inits,ctor,dtor,extras) \
	public: BOOST_PP_SEQ_FOR_EACH(_WOO_ATTR_DECL,thisClass,attrs) _ATTR_TRAITS_CONTAINER_HEADERONLY(thisClass,attrs) \
	thisClass() BOOST_PP_IF(BOOST_PP_SEQ_SIZE(inits attrs),:,) BOOST_PP_SEQ_FOR_EACH_I(_ATTR_MAKE_INITIALIZER,BOOST_PP_DEC(BOOST_PP_SEQ_SIZE(inits attrs)), inits BOOST_PP_SEQ_FOR_EACH(_ATTR_MAKE_INIT_TUPLE,~,attrs)) { ctor ; } /* ctor, with initialization of defaults */ \
	virtual ~thisClass(){ dtor ; }; /* virtual dtor, since classes are polymorphic*/ \
	_WOO_CLASS_BASE_DOC_ATTRS_DEPREC_PY(thisClass,baseClass,makeClassTrait(classTraitSpec),attrs,deprec,extras)





#define WOO_DECL__CLASS_BASE_DOC(args) _WOO_DECL__CLASS_BASE_DOC(args)
#define WOO_IMPL__CLASS_BASE_DOC(args) _WOO_IMPL__CLASS_BASE_DOC(args)
#define _WOO_DECL__CLASS_BASE_DOC(klass,base,doc) _WOO_CLASS_DECLARATION(   klass,base,doc,/*attrs*/,/*deprec*/,/*inits*/,/*ctor*/,/*dtor*/,/*py*/)
#define _WOO_IMPL__CLASS_BASE_DOC(klass,base,doc) _WOO_CLASS_IMPLEMENTATION(klass,base,doc,/*attrs*/,/*deprec*/,/*inits*/,/*ctor*/,/*dtor*/,/*py*/)

#define WOO_DECL__CLASS_BASE_DOC_ATTRS(args) _WOO_DECL__CLASS_BASE_DOC_ATTRS(args)
#define WOO_IMPL__CLASS_BASE_DOC_ATTRS(args) _WOO_IMPL__CLASS_BASE_DOC_ATTRS(args)
#define _WOO_DECL__CLASS_BASE_DOC_ATTRS(klass,base,doc,attrs) _WOO_CLASS_DECLARATION(   klass,base,doc,attrs,/*deprec*/,/*inits*/,/*ctor*/,/*dtor*/,/*py*/)
#define _WOO_IMPL__CLASS_BASE_DOC_ATTRS(klass,base,doc,attrs) _WOO_CLASS_IMPLEMENTATION(klass,base,doc,attrs,/*deprec*/,/*inits*/,/*ctor*/,/*dtor*/,/*py*/)

#define WOO_DECL__CLASS_BASE_DOC_PY(args) _WOO_DECL__CLASS_BASE_DOC_PY(args)
#define WOO_IMPL__CLASS_BASE_DOC_PY(args) _WOO_IMPL__CLASS_BASE_DOC_PY(args)
#define _WOO_DECL__CLASS_BASE_DOC_PY(klass,base,doc,py) _WOO_CLASS_DECLARATION(   klass,base,doc,/*attrs*/,/*deprec*/,/*inits*/,/*ctor*/,/*dtor*/,py)
#define _WOO_IMPL__CLASS_BASE_DOC_PY(klass,base,doc,py) _WOO_CLASS_IMPLEMENTATION(klass,base,doc,/*attrs*/,/*deprec*/,/*inits*/,/*ctor*/,/*dtor*/,py)

#define WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(args) _WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(args)
#define WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(args) _WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(args)
#define _WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(klass,base,doc,attrs,pyExtras) _WOO_CLASS_DECLARATION(   klass,base,doc,attrs,/*deprec*/,/*inits*/,/*ctor*/,/*dtor*/,pyExtras)
#define _WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(klass,base,doc,attrs,pyExtras) _WOO_CLASS_IMPLEMENTATION(klass,base,doc,attrs,/*deprec*/,/*inits*/,/*ctor*/,/*dtor*/,pyExtras)

#define WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR(args) _WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR(args)
#define WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(args) _WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(args)
#define _WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR(klass,base,doc,attrs,ctor) _WOO_CLASS_DECLARATION(   klass,base,doc,attrs,/*deprec*/,/*inits*/,ctor,/*dtor*/,/*pyExtras*/)
#define _WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(klass,base,doc,attrs,ctor) _WOO_CLASS_IMPLEMENTATION(klass,base,doc,attrs,/*deprec*/,/*inits*/,ctor,/*dtor*/,/*pyExtras*/)

#define WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR_PY(args) _WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR_PY(args)
#define WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR_PY(args) _WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR_PY(args)
#define _WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR_PY(klass,base,doc,attrs,ctor,pyExtras) _WOO_CLASS_DECLARATION(   klass,base,doc,attrs,/*deprec*/,/*inits*/,ctor,/*dtor*/,pyExtras)
#define _WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR_PY(klass,base,doc,attrs,ctor,pyExtras) _WOO_CLASS_IMPLEMENTATION(klass,base,doc,attrs,/*deprec*/,/*inits*/,ctor,/*dtor*/,pyExtras)


#define WOO_DECL__CLASS_BASE_DOC_ATTRS_INI_CTOR_PY(args) _WOO_DECL__CLASS_BASE_DOC_ATTRS_INI_CTOR_PY(args)
#define WOO_IMPL__CLASS_BASE_DOC_ATTRS_INI_CTOR_PY(args) _WOO_IMPL__CLASS_BASE_DOC_ATTRS_INI_CTOR_PY(args)
#define _WOO_DECL__CLASS_BASE_DOC_ATTRS_INI_CTOR_PY(klass,base,doc,attrs,ini,ctor,pyExtras) _WOO_CLASS_DECLARATION(   klass,base,doc,attrs,/*deprec*/,ini,ctor,/*dtor*/,pyExtras)
#define _WOO_IMPL__CLASS_BASE_DOC_ATTRS_INI_CTOR_PY(klass,base,doc,attrs,ini,ctor,pyExtras) _WOO_CLASS_IMPLEMENTATION(klass,base,doc,attrs,/*deprec*/,ini,ctor,/*dtor*/,pyExtras)

#define WOO_DECL__CLASS_BASE_DOC_ATTRS_INI_CTOR_DTOR_PY(args) _WOO_DECL__CLASS_BASE_DOC_ATTRS_INI_CTOR_DTOR_PY(args)
#define WOO_IMPL__CLASS_BASE_DOC_ATTRS_INI_CTOR_DTOR_PY(args) _WOO_IMPL__CLASS_BASE_DOC_ATTRS_INI_CTOR_DTOR_PY(args)
#define _WOO_DECL__CLASS_BASE_DOC_ATTRS_INI_CTOR_DTOR_PY(klass,base,doc,attrs,ini,ctor,dtor,pyExtras) _WOO_CLASS_DECLARATION(   klass,base,doc,attrs,/*deprec*/,ini,ctor,dtor,pyExtras)
#define _WOO_IMPL__CLASS_BASE_DOC_ATTRS_INI_CTOR_DTOR_PY(klass,base,doc,attrs,ini,ctor,dtor,pyExtras) _WOO_CLASS_IMPLEMENTATION(klass,base,doc,attrs,/*deprec*/,ini,ctor,dtor,pyExtras)

#define WOO_DECL__CLASS_BASE_DOC_ATTRS_DEPREC_INI_CTOR_DTOR_PY(args) _WOO_DECL__CLASS_BASE_DOC_ATTRS_DEPREC_INI_CTOR_DTOR_PY(args)
#define WOO_IMPL__CLASS_BASE_DOC_ATTRS_DEPREC_INI_CTOR_DTOR_PY(args) _WOO_IMPL__CLASS_BASE_DOC_ATTRS_DEPREC_INI_CTOR_DTOR_PY(args)
#define _WOO_DECL__CLASS_BASE_DOC_ATTRS_DEPREC_INI_CTOR_DTOR_PY(klass,base,doc,attrs,deprec,ini,ctor,dtor,pyExtras) _WOO_CLASS_DECLARATION(   klass,base,doc,attrs,deprec,ini,ctor,dtor,pyExtras)
#define _WOO_IMPL__CLASS_BASE_DOC_ATTRS_DEPREC_INI_CTOR_DTOR_PY(klass,base,doc,attrs,deprec,ini,ctor,dtor,pyExtras) _WOO_CLASS_IMPLEMENTATION(klass,base,doc,attrs,deprec,ini,ctor,dtor,pyExtras)

#define WOO_CLASS_DECLARATION(allArgsTogether) _WOO_CLASS_DECLARATION(allArgsTogether)

#define _WOO_CLASS_DECLARATION(thisClass,baseClass,classTraitSpec,attrs,deprec,inits,ctor,dtor,pyExtras) \
	/* class itself */	REGISTER_CLASS_AND_BASE(thisClass,baseClass) \
	/* attribute declarations */ BOOST_PP_SEQ_FOR_EACH(_WOO_ATTR_DECL,thisClass,attrs) \
	/* trait definitions*/ /* BOOST_PP_SEQ_FOR_EACH(_WOO_TRAIT_DECL,thisClass,attrs) */ \
	/* later: call postLoad via ADL*/ public: void callPostLoad(void* addr) override { baseClass::callPostLoad(addr); postLoad(*this,addr); } \
	/* accessors for deprecated attributes, with warnings */ BOOST_PP_SEQ_FOR_EACH(_ACCESS_DEPREC,thisClass,deprec) \
	/**follow pure declarations of which implementation is handled sparately**/ \
	/*1. ctor declaration */ thisClass(); \
	/*2. dtor declaration */ virtual ~thisClass(); \
	/*3. traits container*/ _ATTR_TRAITS_CONTAINER_DECL(thisClass); \
	/*4. boost::serialization declarations */ _WOO_BOOST_SERIALIZE_DECL(thisClass,baseClass,attrs) \
	/*5. set attributes from kw ctor */ protected: void pySetAttr(const std::string& key, const py::object& value) override; \
	/*6. for pickling*/ py::dict pyDict(bool all=true) const override; \
	/*7. python class registration*/ std::function<void()> pyRegisterClass(py::module_& mod) override; \
	/*8. ensures v-table; will be removed later*/ void must_use_both_WOO_CLASS_BASE_DOC_ATTRS_and_WOO_PLUGIN(); \
	/*9.*/ void must_use_both_WOO_CLASS_DECLARATION_and_WOO_CLASS_IMPLEMENTATION(); \
	public: /* make the rest public by default again */

#define WOO_CLASS_IMPLEMENTATION(allArgsTogether) _WOO_CLASS_IMPLEMENTATION(allArgsTogether)

#define _WOO_CLASS_IMPLEMENTATION(thisClass,baseClass,classTraitSpec,attrs,deprec,init,ctor,dtor,pyExtras) \
	/*1.*/ thisClass::thisClass() BOOST_PP_IF(BOOST_PP_SEQ_SIZE(attrs init),:,) BOOST_PP_SEQ_FOR_EACH_I(_ATTR_MAKE_INITIALIZER,BOOST_PP_DEC(BOOST_PP_SEQ_SIZE(attrs init)), BOOST_PP_SEQ_FOR_EACH(_ATTR_MAKE_INIT_TUPLE,~,attrs) init) { ctor; } \
	/*2.*/ thisClass::~thisClass(){ dtor; } \
	/*3.*/ _ATTR_TRAITS_CONTAINER_IMPL(thisClass,attrs)  \
	/*4.*/ _WOO_BOOST_SERIALIZE_IMPL(thisClass,baseClass,attrs) \
	/*5.*/ void thisClass::pySetAttr(const std::string& key, const py::object& value){ BOOST_PP_SEQ_FOR_EACH(_PYSET_ATTR,thisClass,attrs); BOOST_PP_SEQ_FOR_EACH(_PYSET_ATTR_DEPREC,thisClass,deprec); baseClass::pySetAttr(key,value); } \
	/*6.*/ py::dict thisClass::pyDict(bool all) const { py::dict ret; BOOST_PP_SEQ_FOR_EACH(_PYDICT_ATTR,~,attrs); WOO_PY_DICT_UPDATE(baseClass::pyDict(all),ret); return ret; } \
	/*7.*/ std::function<void()> thisClass::pyRegisterClass(py::module_& mod) { _PY_REGISTER_CLASS_BODY(thisClass,baseClass,makeClassTrait(classTraitSpec),attrs,deprec,pyExtras); } \
	/*8. -- handled by WOO_PLUGIN */ \
	/*9.*/ void thisClass::must_use_both_WOO_CLASS_DECLARATION_and_WOO_CLASS_IMPLEMENTATION(){};

	


/* this used to be in lib/factory/Factorable.hpp */
#define REGISTER_CLASS_AND_BASE(cn,bcn) public: EIGEN_MAKE_ALIGNED_OPERATOR_NEW ; virtual string getClassName() const override { return #cn; }; public: virtual vector<string> getBaseClassNames() const override { return {#bcn}; }


// this disabled exposing the vector as python list
// it must come before pybind11 kicks in, and outside of any namespaces
// definition of a special container type is in lib/pyutil/converters.hpp
PYBIND11_MAKE_OPAQUE(std::vector<shared_ptr<woo::Object>>)

namespace woo{

struct Object: public boost::noncopyable, public enable_shared_from_this<Object> {
	// http://www.boost.org/doc/libs/1_49_0/libs/smart_ptr/sp_techniques.html#static
	struct null_deleter{void operator()(void const *)const{}};
	static vector<py::object> derivedCxxClasses;
	static py::list getDerivedCxxClasses();
	// this is only for informative purposes, hence the typecast is OK
	ptrdiff_t pyCxxAddr() const{ return (ptrdiff_t)this; }

		template <class ArchiveT> void serialize(ArchiveT & ar, unsigned int version){ };
		// lovely cast members like in eigen :-)
		template <class DerivedT> const DerivedT& cast() const { return *static_cast<const DerivedT*>(this); }
		template <class DerivedT> DerivedT& cast(){ return *static_cast<DerivedT*>(this); }
		// testing instance type
		template <class DerivedT> bool isA() const { return (bool)dynamic_cast<const DerivedT*>(this); }

		static shared_ptr<Object> boostLoad(const string& f){ auto obj=make_shared<Object>(); ObjectIO::load(f,"woo__Object",obj); return obj; }
		virtual void boostSave(const string& f){ auto sh(shared_from_this()); ObjectIO::save(f,"woo__Object",sh); }
		//template<class DerivedT> shared_ptr<DerivedT> _cxxLoadChecked(const string& f){ auto obj=_cxxLoad(f); auto obj2=dynamic_pointer_cast<DerivedT>(obj); if(!obj2) throw std::runtime_error("Loaded type "+obj->getClassName()+" could not be cast to requested type "+DerivedT::getClassNameStatic()); }

		Object() {};
		virtual ~Object() {};
		// comparison of strong equality of 2 objects (by their address)
		bool operator==(const Object& other) const { return this==&other; }
		bool operator!=(const Object& other) const { return this!=&other; }

		void pyUpdateAttrs(const py::dict& d);
		//static void pyUpdateAttrs(const shared_ptr<Object>&, const py::dict& d);

		virtual void pySetAttr(const std::string& key, const py::object& value){ woo::AttributeError("No such attribute: "+key+".");};
		virtual py::dict pyDict(bool all=true) const { return py::dict(); }
		virtual void callPostLoad(void* addr){ postLoad(*this,addr); }
		// check whether the class registers itself or whether it calls virtual function of some base class;
		// that means that the class doesn't register itself properly
		virtual void checkPyClassRegistersItself(const std::string& thisClassName) const;
		// perform class registration; overridden in all classes
		virtual std::function<void()> pyRegisterClass(py::module_& mod);
		// perform any manipulation of arbitrary constructor arguments coming from python, manipulating them in-place;
		// the remainder is passed to the Object_ctor_kwAttrs of the respective class (note: args must be empty)
		virtual void pyHandleCustomCtorArgs(py::args_& args, py::kwargs& kw){ return; }
		
		//! string representation of this object
		virtual std::string pyStr() const { return "<"+getClassName()+" @ "+ptr_to_string(this)+">"; }
	
	// overridden by REGISTER_CLASS_BASE_BASE in derived classes
	virtual string getClassName() const { return "Object"; }
	virtual vector<string> getBaseClassNames() const{ return {}; }

	std::string getBaseClassName(unsigned int i=0) const { std::vector<std::string> bases(getBaseClassNames()); return (i>=bases.size()?std::string(""):bases[i]); } 
	int getBaseClassNumber(){ return getBaseClassNames().size(); }
};

// helper functions
template <typename T>
shared_ptr<T> Object_ctor_kwAttrs(py::args_& t, py::kwargs& d){
	shared_ptr<T> instance=make_shared<T>();
	instance->pyHandleCustomCtorArgs(t,d); // can change t and d in-place
	if(py::len(t)>0) throw std::runtime_error("Zero (not "+to_string(py::len(t))+") non-keyword constructor arguments required [in Object_ctor_kwAttrs; Object::pyHandleCustomCtorArgs might had changed it after your call].");
	if(py::len(d)>0) instance->pyUpdateAttrs(d);
	instance->callPostLoad(NULL); 
	return instance;
}

template<typename T>
shared_ptr<T> Object__setstate__(py::dict state){
	shared_ptr<T> instance=make_shared<T>();
	instance->pyUpdateAttrs(state);
	instance->callPostLoad(NULL);
	return instance;
}
// #endif

}; /* namespace woo */

#ifdef WOO_CEREAL
	CEREAL_FORCE_DYNAMIC_INIT(Object);
#endif

