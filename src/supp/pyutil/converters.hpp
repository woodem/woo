#pragma once 

#include<vector>
#include<list>

#include<woo/lib/base/openmp-accu.hpp>

#include<pybind11/stl.h>
#include<pybind11/stl_bind.h>
// no-op with pybind11
namespace woo{
	template<typename T> void converters_cxxVector_pyList_2way(py::module& mod, const string& name=""){
		//std::cerr<<"Binding vec_"<<typeid(T).name()<<"..."<<std::endl;
		string name2=name.empty()?(string("_")+(typeid(typename T::element_type).name())+"List"):name;
		auto vec=py::bind_vector<std::vector<T>>(mod,name2,py::module_local(false));
		// pickling will be used **only** for types which were declared opaque
		// but we declare it for all types anyway
		vec.def(py::pickle(
				// __getstate__ 
				[](const vector<T>& self){
				py::list ret; for(const auto& item: self) ret.append(item);
				return ret;
			},
			// __setstate__
			[](py::list lst){
				vector<T> ret(py::len(lst));
				for(int i=0; i<py::len(lst); i++) ret[i]=lst[i].cast<T>();
				return ret;
			}
		));
		//std::cerr<<"Declaring py::list → vec_"<<typeid(T).name()<<" implicit convertibility..."<<std::endl;
		py::implicitly_convertible<py::list,std::vector<T>>();
		py::implicitly_convertible<py::tuple,std::vector<T>>();
	};
};

#if 0
namespace pybind11::detail {
	#if 0
	template<typename T> struct type_caster<OpenMPAccumulator<T>>{
		PYBIND11_TYPE_CASTER(OpenMPArrayAccumulator<T>,"OpenMPArrayAccumulator");
		static handle cast(OpenMPArrayAccumulator<T> src, return_value_policy policy, handle parent){
			return py::cast(src.i);
		}
	};
	#endif
	template<typename T> struct type_caster<OpenMPArrayAccumulator<T>>{
		PYBIND11_TYPE_CASTER(OpenMPArrayAccumulator<T>,"OpenMPArrayAccumulator");
		static handle cast(OpenMPArrayAccumulator<T> src, return_value_policy policy, handle parent){
			std::vector<T> ret(src.size());
			for(size_t i=0; i<src.size(); i++) ret[i]=src[i];
			return py::cast(ret);
		}
	};
}
#endif

