#pragma once


#ifdef WOO_NANOBIND
	#define PY_NAMESPACE nanobind
	#include <nanobind/nanobind.h>
	#include <nanobind/operators.h>
	#include <nanobind/trampoline.h>
	#include <nanobind/ndarray.h>
	#include <nanobind/eval.h>
	#include <nanobind/stl/map.h>
	#include <nanobind/stl/vector.h>
	#include <nanobind/stl/list.h>
	#include <nanobind/stl/set.h>
	#include <nanobind/stl/string.h>
	#include <nanobind/stl/shared_ptr.h>

	#define PYBIND11_OVERRIDE(RET_TYPE,PARENT_CLASS,METHOD_NAME,...) NB_OVERRIDE(METHOD_NAME,__VA_ARGS__)
	#define PYBIND11_OVERRIDE_PURE(RET_TYPE,PARENT_CLASS,METHOD_NAME,...) NB_OVERRIDE_PURE(METHOD_NAME,__VA_ARGS__)
	#define PYBIND11_OVERRIDE_PURE_NAME(RET_TYPE,PARENT_CLASS,METHOD_NAME,...) NB_OVERRIDE_PURE_NAME(METHOD_NAME,__VA_ARGS__)
	#define PYBIND11_MAKE_OPAQUE NB_MAKE_OPAQUE
	#define PYBIND11_MODULE NB_MODULE
	#define def_readwrite def_rw
	#define def_readonly def_ro
	#define def_readonly_static def_ro_static
	#define def_property def_prop_rw
	#define def_property_readonly def_prop_ro
	#define def_property_readonly_static def_prop_ro_static
	#define register_exception exception
	#define return_value_policy rv_policy
	#define error_already_set python_error
	#define reinterpret_borrow borrow
	#define reinterpret_steal steal
	#define PY_SHARED_PTR_HOLDER(T)
	#define PY_CAST(a,b) py::cast<a>(b)
	#define PY_PICKLE(getstate,setstate) .def("__getstate__",getstate).def("__setstate__",setstate)

	namespace nanobind{
		inline module_ create_extension_module(const char *name, const char *doc,  PyModuleDef *def){
			new (def) PyModuleDef{/* m_base */ PyModuleDef_HEAD_INIT, /* m_name */ name, /* m_doc */ doc, /* m_size */ -1, /* m_methods */ nullptr, /* m_slots */ nullptr, /* m_traverse */ nullptr, /* m_clear */ nullptr, /* m_free */ nullptr};
			auto *m = PyModule_Create(def);
			if (m == nullptr) {
				if (PyErr_Occurred()) { throw python_error(); }
				detail::fail("Internal error in ::create_extension_module()");
			}
			// TODO: Should be reinterpret_steal for Python 3, but Python also steals it again when
			//       returned from PyInit_...
			//       For Python 2, reinterpret_borrow was correct.
			return borrow<module_>(m);
		}
	}
#else
	#define PY_NAMESPACE pybind11
	#define PYBIND11_DETAILED_ERROR_MESSAGES
	// includes python headers, which also define PY_MAJOR_VERSION
	#include<pybind11/pybind11.h>
	#include<pybind11/eval.h>
	#include<pybind11/operators.h>
	#include<pybind11/stl.h>
	#include<pybind11/stl_bind.h>
	// #include<pybind11/eigen.h>
	#define PY_SHARED_PTR_HOLDER(T) , std::shared_ptr<T>
	#define import_ import
	#define PY_CAST(a,b) b.cast<a>()
	#define PY_PICKLE(getstate,setstate) .def(py::pickle(getstate,setstate))
	namespace pybind11{
		constexpr auto create_extension_module = pybind11::module_::create_extension_module;
	}
	#define NB_TRAMPOLINE(parent,size) using parent::parent;
#endif

namespace py=PY_NAMESPACE;

#include<functional>

#define WOO_PY_DICT_UPDATE(src,dst) { for(auto kv: src) dst[kv.first]=kv.second; }

// pybind11 needs args separated
#define WOO_PY_ARGS(...) __VA_ARGS__
#ifndef PYBIND11_VERSION_HEX
	#define PYBIND11_VERSION_HEX ((PYBIND11_VERSION_MAJOR<<(3*8))|(PYBIND11_VERSION_MINOR<<(2*8))|(PYBIND11_VERSION_PATCH<<(1*8)))
#endif

// forward decl
namespace woo { struct Object; }; 

namespace PY_NAMESPACE{
	// emulate boost::python::extract
	template<typename T>
	struct __attribute__((visibility("hidden"))) extract{
		const object& obj;
		extract(const object& obj_): obj(obj_){}
		/* this is colosally ugly, but unfortunately py::isinstance only checks real instance type, not convertibility */
		// bool check(){ return isinstance<T>(obj); }
		bool check(){ try{ this->operator()(); return true; } catch(...){ return false; }  }
		operator T() const { return this->operator()(); }
		T operator()() const {
			// if constexpr(std::is_convertible_v<T,std::string>) return std::string(str(obj));
			return cast<T>(obj);
		}
	};
	// boost::python::ptr does not seem to be needed in pybind11 when casting from raw pointer
	// make it no-op
	template<typename T>
	T* ptr(T* p){ return p; }
	// bp::import → py::module_::import
	inline module_ import_(const char *name){ return module_::import_(name); }
	// bp::scope → py::module
	typedef args args_;
	namespace api{
		inline void delitem(::PY_NAMESPACE::dict& d, const char* key){ d.attr("pop")(key); }
	}
}

#ifdef WOO_NANOBIND
	std::ostream& operator<<(std::ostream& os, const py::str& s) { os<<s.c_str(); return os; }
#endif

#include<spdlog/fmt/ostr.h>
template<>
struct fmt::formatter<py::str> : fmt::ostream_formatter {};
