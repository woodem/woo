#pragma once 

#include<vector>
#include<list>

#include"../base/openmp-accu.hpp"
#include"compat.hpp"

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

#if 1
namespace pybind11::detail {
	template<typename T> struct type_caster<OpenMPAccumulator<T>>{
		// static_assert(std::is_integral<T>() || std::is_floating_point<T>());
		#if PYBIND11_VERSION_HEX>=0x02090000
			PYBIND11_TYPE_CASTER(OpenMPAccumulator<T>,const_name("OpenMPAccumulator"));
		#else
			PYBIND11_TYPE_CASTER(OpenMPAccumulator<T>,_("OpenMPAccumulator"));
		#endif
		static handle cast(const OpenMPAccumulator<T>& src, return_value_policy policy, handle parent){
			return py::cast(src.get());
			#if 0
				if constexpr(std::is_integral<T>()) return PyLong_FromLong((long)src.get());
				if constexpr(std::is_floating_point<T>()) return PyFloat_FromDouble((double)src.get());
				else throw std::runtime_error("Error converting OpenMPAccu to number.");
			#endif
		}
		bool load(handle src, bool){
			// setting array value not supported
			return false;
		}
	};
	template<typename T> struct type_caster<OpenMPArrayAccumulator<T>>{
		static_assert(std::is_integral<T>() || std::is_floating_point<T>());

		#if PYBIND11_VERSION_HEX>=0x02090000
			PYBIND11_TYPE_CASTER(OpenMPArrayAccumulator<T>,const_name("OpenMPArrayAccumulator"));
		#else
			PYBIND11_TYPE_CASTER(OpenMPArrayAccumulator<T>,_("OpenMPArrayAccumulator"));
		#endif

		static handle cast(const OpenMPArrayAccumulator<T>& src, return_value_policy policy, handle parent){
			PyObject *ret=PyList_New(src.size());
			auto data=src.getPerThreadData();
			#if 0
				for(size_t i=0; i<data.size(); i++){ for(size_t j=0; j<data[i].size(); j++){ std::cerr<<" "<<data[i][j]; } std::cerr<<";"; } std::cerr<<endl;
			#endif
			for(size_t i=0; i<src.size(); i++){
				if constexpr(std::is_integral<T>()) PyList_SetItem(ret,i,PyLong_FromLong((long)src.get(i)));
				else if constexpr(std::is_floating_point<T>()) PyList_SetItem(ret,i,PyFloat_FromDouble((double)src.get(i)));
				else throw std::runtime_error("Error converting OpenMPArrayAccu to sequence?");
			}
			return ret;
			//std::vector<T> ret(src.size());
			//for(size_t i=0; i<src.size(); i++){ ret[i]=src[i]; std::cerr<<"  item ["<<i<<"]: "<<src[i]<<std::endl; }
			//return py::cast(ret);
		}
		bool load(handle src, bool){
			// setting array value not supported
			return false;
		#if 0
			PyObject *source=src.ptr();
			if(std::is_integral<T>()){
				PyObject *tmp=PyNumber_Long(source);
				if(!tmp) return false; // throw std::runtime_error("OpenMPArrayAccumulator: failed to extract integral number.");
				value.set(PyLong_AsLong(tmp));
				Py_DECREF(tmp);
				return !PyErr_Occurred();
			};
			if(std::is_integral<T>()){
				PyObject *fp=PyNumber_Float(source);
				PyObject *lo=PyNumber_Long(source);
				if(fp){ value.set(PyFloat_AsDouble(fp)); Py_DECREF(fp); }
				else if(lo){ value.set(PyLong_AsLong(lo)); Py_DECREF(lo); }
				else return false;
				return !PyErr_Occurred();
			};
		#endif
		}
	};
}
#endif

