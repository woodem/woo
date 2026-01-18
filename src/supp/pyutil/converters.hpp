#pragma once 

#include<vector>
#include<list>

#include"../base/openmp-accu.hpp"
#include"compat.hpp"

#ifdef WOO_NANOBIND
namespace woo{
	template<typename T> void converters_cxxVector_pyList_2way(py::module_& mod, const string& name=""){
		std::cerr<<"TODO: converters_cxxVector_pyList_2way: no-op with nanobind"<<std::endl;
	}
}
#else
namespace woo{
	template<typename T> void converters_cxxVector_pyList_2way(py::module_& mod, const string& name=""){
		//std::cerr<<"Binding vec_"<<typeid(T).name()<<"..."<<std::endl;
		string name2=name.empty()?(string("_")+(typeid(typename T::element_type).name())+"List"):name;
		auto vec=py::bind_vector<std::vector<T>>(mod,name2
			#ifndef WOO_NANOBIND
				,py::module_local(false)
			#endif
		);
		// pickling will be used **only** for types which were declared opaque
		// but we declare it for all types anyway
		vec PY_PICKLE(
				// __getstate__ 
				[](const vector<T>& self){
				py::list ret; for(const auto& item: self) ret.append(item);
				return ret;
			},
			// __setstate__
			[](py::list lst){
				vector<T> ret(py::len(lst));
				for(int i=0; i<py::len(lst); i++) ret[i]=PY_CAST(T,lst[i]);
				return ret;
			}
		);
		//std::cerr<<"Declaring py::list → vec_"<<typeid(T).name()<<" implicit convertibility..."<<std::endl;
		py::implicitly_convertible<py::list,std::vector<T>>();
		py::implicitly_convertible<py::tuple,std::vector<T>>();
	};
};
#endif


#if 0
namespace nanobind::detail{
	template<typename T> struct type_caster<OpenMPAccumulator<T>>{
		bool from_python(handle src, uint8_t flags, cleanup_list *cleanup) noexcept {

		}
		static handle cast(const OpenMPAccumulator<T>& src, return_value_policy policy, handle parent){
			return py::cast(src.get()).release();
		}
		bool load(handle src, bool){
			// setting array value not supported
			return false;
		}
	};
}
#endif

#ifndef WOO_NANOBIND
namespace pybind11::detail {
	template<typename T> struct type_caster<OpenMPAccumulator<T>>{
		// static_assert(std::is_integral<T>() || std::is_floating_point<T>());
		PYBIND11_TYPE_CASTER(OpenMPAccumulator<T>,const_name("OpenMPAccumulator"));
		static handle cast(const OpenMPAccumulator<T>& src, return_value_policy policy, handle parent){
			return py::cast(src.get()).release();
		}
		bool load(handle src, bool){
			// setting array value not supported
			return false;
		}
	};
	template<typename T> struct type_caster<OpenMPArrayAccumulator<T>>{
		static_assert(std::is_integral<T>() || std::is_floating_point<T>());
		PYBIND11_TYPE_CASTER(OpenMPArrayAccumulator<T>,const_name("OpenMPArrayAccumulator"));
		static handle cast(const OpenMPArrayAccumulator<T>& src, return_value_policy policy, handle parent){
			PyObject *ret=PyList_New(src.size());
			for(size_t ix=0; ix<src.size(); ix++){
				PyList_SetItem(ret,ix,py::cast(src.get(ix)).release().ptr());
			}
			return ret;
		}
		bool load(handle src, bool){ // setting array value not supported
			return false;
		}
	};
}
#endif

