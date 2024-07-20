#pragma once

#include<functional>
#define add_property_readonly def_property_readonly
#define add_property def_property
#define add_static_property def_property_static
// #define staticmethod(name) def("__" name,[](const auto& o){ return 0; })
#define staticmethod(name)
//def("__boost_python_deprecated_filler_" name,[](const auto& o){ return 0; })
#define WOO_PY_DICT_CONTAINS(dict,key) dict.contains(key)
#define WOO_PY_DICT_UPDATE(src,dst) { for(auto kv: src) dst[kv.first]=kv.second; }

// pybind11 needs args separated
#define WOO_PY_ARGS(...) __VA_ARGS__
#define WOO_PY_GETTER_COPY(func) py::cpp_function(func,py::return_value_policy::copy)
#define WOO_PY_EXPOSE_COPY(classT,attr) [](const classT& o){return o.*(attr);},py::return_value_policy::copy
#define WOO_PY_RETURN__TAKE_OWNERSHIP py::return_value_policy::take_ownership

#ifndef PYBIND11_VERSION_HEX
	#define PYBIND11_VERSION_HEX ((PYBIND11_VERSION_MAJOR<<(3*8))|(PYBIND11_VERSION_MINOR<<(2*8))|(PYBIND11_VERSION_PATCH<<(1*8)))
#endif
// forward decl
namespace woo { struct Object; }; 
#define PYBIND11_DETAILED_ERROR_MESSAGES
#include<pybind11/pybind11.h>
namespace pybind11{
	// emulate boost::python::extract
	template<typename T>
	struct __attribute__((visibility("hidden"))) extract{
		const object& obj;
		extract(const object& obj_): obj(obj_){}
		/* this is colosally ugly, but unfortunately py::isinstance only checks real instance type, not convertibility */
		// bool check(){ return isinstance<T>(obj); }
		bool check(){ try{ cast<T>(obj); return true; } catch(...){ return false; }  }
		T operator()() const { return cast<T>(obj); }
		operator T() const { return cast<T>(obj); }
	};
	// boost::python::ptr does not seem to be needed in pybind11 when casting from raw pointer
	// make it no-op
	template<typename T>
	T* ptr(T* p){ return p; }
	// bp::import → py::module::import
	inline module import(const char *name){ return module::import(name); }
	// bp::scope → py::module
	typedef args args_;
	typedef module module_;
	namespace api{
		inline void delitem(::pybind11::dict& d, const char* key){ d.attr("pop")(key); }
	}
}
