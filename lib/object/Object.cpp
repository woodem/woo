#include<woo/lib/object/Object.hpp>
#include<woo/lib/pyutil/converters.hpp>


#if 0
static void Object_setAttr(py::object self, py::str name, py::object value){
#if 0
	py::dict d=py::extract<py::dict>(py::getattr(self,"__dict__"));
	if(d.has_key(name))
#endif
	{ py::setattr(self,name,value); return; }
#if 0
	woo::AttributeError(("Class "+py::extract<std::string>(py::getattr(py::getattr(self,"__class__"),"__name__"))()+" does not have attribute "+py::extract<std::string>(name)()+".").c_str());
#endif
}
#endif

#ifdef WOO_CEREAL
	CEREAL_REGISTER_TYPE(Object); CEREAL_REGISTER_DYNAMIC_INIT(Object);
#endif

namespace woo {

vector<py::object> Object::derivedCxxClasses;
py::list Object::getDerivedCxxClasses(){ py::list ret; for(py::object c: derivedCxxClasses) ret.append(c); return ret; }

py::dict Object_pyDict_pickleable(const Object& o) { return o.pyDict(/*all*/false); }

std::function<void()> Object::pyRegisterClass(py::module_& mod) {
	checkPyClassRegistersItself("Object");
	#ifdef WOO_PYBIND11
		py::class_<Object,shared_ptr<Object>> classObj(mod,"Object","Base class for all Woo classes, providing uniform interface for constructors with attributes, attribute access, pickling, serialization via cereal/boost::serialization, equality comparison, attribute traits.");
	#else
		py::class_<Object, shared_ptr<Object>, boost::noncopyable > classObj("Object","Base class for all Woo classes, providing uniform interface for constructors with attributes, attribute access, pickling, serialization via cereal/boost::serialization, equality comparison, attribute traits.");
	#endif
	return [classObj,mod]() mutable {
		classObj
		#ifdef WOO_PYBIND11
			.def(py::init<>())
			// this one is declared in all derived classes; needed here??
			//.def(py::init([](py::args_& a,py::kwargs& kw){ return Object_ctor_kwAttrs<thisClass>(a,kw); }))
		#endif
		.def("__str__",&Object::pyStr).def("__repr__",&Object::pyStr)
		.def("dict",&Object::pyDict,WOO_PY_ARGS(py::arg("all")=true),"Return dictionary of attributes; *all* will cause also attributed with the ``noSave`` or ``noDump`` flags to be returned.")
		//.def("_attrTraits",&Object::pyAttrTraits,WOO_PY_ARGS(py::arg("parents")=true),"Return list of attribute traits.")
		.def("updateAttrs",&Object::pyUpdateAttrs,"Update object attributes from given dictionary")
		#ifdef WOO_PYBIND11
			.def(py::pickle([](const shared_ptr<Object>& self){ return self->pyDict(/*all*/false); },&Object__setstate__<Object>))
		#else
			/* boost::python pickling support, as per http://www.boost.org/doc/libs/1_42_0/libs/python/doc/v2/pickle.html */ 
			.def("__getstate__",Object_pyDict_pickleable).def("__setstate__",&Object::pyUpdateAttrs)
			.add_property_readonly("__safe_for_unpickling__",&Object::getClassName,"just define the attr, return some bogus data")
			.add_property_readonly("__getstate_manages_dict__",&Object::getClassName,"just define the attr, return some bogus data")
		#endif
		.def("save",&Object::boostSave,py::arg("filename"))
		.def_static("_boostLoad",&Object::boostLoad,py::arg("filename")) 
		//.def_readonly("_derivedCxxClasses",&Object::derivedCxxClasses)
		#ifdef WOO_PYBIND11
			.def_property_readonly_static("_derivedCxxClasses",[](py::object){ return Object::getDerivedCxxClasses(); })
		#else
			.def_property_readonly_static("_derivedCxxClasses",&Object::getDerivedCxxClasses)
		#endif
		.def_property_readonly("_cxxAddr",&Object::pyCxxAddr)
		// setting attributes with protection of creating class instance mistakenly
		#if 0
			.def("__setattr__",&Object_setAttr)
		#endif
		// constructor with dictionary of attributes
		//.def("__init__",py::raw_constructor(Object_ctor_kwAttrs<Object>))
		// comparison operators
		.def(py::self == py::self)
		.def(py::self != py::self)


		;
		classObj.attr("_attrTraits")=py::list();
		// repeat the docstring here

		woo::converters_cxxVector_pyList_2way<shared_ptr<Object>>(mod);

		shared_ptr<ClassTrait> traitPtr=make_shared<ClassTrait>();
		traitPtr->name("Object").doc(py::extract<string>(classObj.attr("__doc__"))()).file(__FILE__).line(__LINE__);
		classObj.attr("_classTrait")=traitPtr;
	};
	//classObj.attr("_derivedCxxClasses")=Object::derivedCxxClasses;
}

void Object::checkPyClassRegistersItself(const std::string& thisClassName) const {
	if(getClassName()!=thisClassName) throw std::logic_error(("Class "+getClassName()+" does not register with WOO_CLASS_BASE_DOC_ATTR*, would not be accessible from python.").c_str());
}

void Object::pyUpdateAttrs(const py::dict& d){
	#ifdef WOO_PYBIND11
	for(auto kv: d){
		pySetAttr(py::cast<string>(kv.first),py::object(py::reinterpret_borrow<py::object>(kv.second)));
	}
	#else
	py::list l=d.items(); size_t ll=py::len(l); if(ll==0) return;
	for(size_t i=0; i<ll; i++){
		py::tuple t=py::extract<py::tuple>(l[i]);
		string key=py::extract<string>(t[0]);
		pySetAttr(key,t[1]);
	}
	#endif
	callPostLoad(NULL);
}

};
