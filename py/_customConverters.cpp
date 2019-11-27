// 2009 © Václav Šmilauer <eudoxos@arcig.cz>

#ifdef WOO_PYBIND11
#include<woo/lib/base/Types.hpp>
#include<woo/lib/pyutil/compat.hpp>
#include<woo/core/Master.hpp>

#include<woo/lib/object/Object.hpp>
#include<woo/core/Field.hpp>
#include<woo/pkg/dem/Contact.hpp>
#include<woo/pkg/dem/Particle.hpp>

// #include<pybind11/eigen.h>
#include<pybind11/stl.h>

PYBIND11_MAKE_OPAQUE(std::vector<shared_ptr<Node>>)
PYBIND11_MAKE_OPAQUE(std::vector<shared_ptr<Particle>>)
PYBIND11_MAKE_OPAQUE(std::vector<shared_ptr<Object>>)

WOO_PYTHON_MODULE(_customConverters);
PYBIND11_MODULE(_customConverters,mod){
	mod.attr("__name__")="woo._customConverters";
	std::cerr<<"WARN: importing stub woo._customConverters (not yet implemented in pybind11)."<<std::endl;

	#define _OPAQUE_LIST_EXPOSE(Type) py::class_<vector<shared_ptr<Type>>>(mod, #Type "List").def(py::init<>());
		_OPAQUE_LIST_EXPOSE(Node);
		_OPAQUE_LIST_EXPOSE(Particle);
		_OPAQUE_LIST_EXPOSE(Object);
	#undef _OPAQUE_LIST_EXPOSE
};

#else

// this is not currently used, but can be enabled if needed
// probably breaks compilation for older (like <=1.35 or so)
// boost::python
#if 0
	#include<indexing_suite/container_suite.hpp>
	// #include<indexing_suite/container_proxy.hpp>
	#include<indexing_suite/vector.hpp>
#endif

#if 0
	#include<woo/lib/pyutil/numpy.hpp>
#endif


#include<cmath> // workaround for http://boost.2283326.n4.nabble.com/Boost-Python-Compile-Error-s-GCC-via-MinGW-w64-tp3165793p3166760.html
#include<boost/python.hpp>
#include<boost/python/class.hpp>
#include<boost/python/module.hpp>

#include<vector>
#include<string>
#include<stdexcept>
#include<iostream>
#include<map>

#include<woo/lib/base/Types.hpp>

#include<woo/lib/base/Math.hpp>
#include<woo/lib/base/openmp-accu.hpp>
#include<woo/lib/pyutil/converters.hpp>
#include<woo/core/Scene.hpp>
#include<woo/core/MatchMaker.hpp>
#include<woo/pkg/dem/Particle.hpp> // for exposing ParticleList




#include <boost/python/suite/indexing/vector_indexing_suite.hpp>



// increc magic from https://wiki.python.org/moin/boost.python/HowTo#Method_2
template<typename EigenMatrix>
struct custom_OpenMPAccumulator_to_EigenMatrix{ static PyObject* convert(const OpenMPAccumulator<EigenMatrix>& acc){ return py::incref(py::object(acc.get()).ptr()); } };

struct custom_OpenMPAccumulator_to_float{ static PyObject* convert(const OpenMPAccumulator<Real>& acc){ return py::incref(PyFloat_FromDouble(acc.get())); } };
struct custom_OpenMPAccumulator_from_float{
	custom_OpenMPAccumulator_from_float(){  py::converter::registry::push_back(&convertible,&construct,py::type_id<OpenMPAccumulator<Real> >()); }
	static void* convertible(PyObject* obj_ptr){ return PyFloat_Check(obj_ptr) ? obj_ptr : 0; }
	static void construct(PyObject* obj_ptr, py::converter::rvalue_from_python_stage1_data* data){ void* storage=((py::converter::rvalue_from_python_storage<OpenMPAccumulator<Real> >*)(data))->storage.bytes; new (storage) OpenMPAccumulator<Real>; ((OpenMPAccumulator<Real>*)storage)->set(py::extract<Real>(obj_ptr)); data->convertible=storage; }
};

struct custom_OpenMPAccumulator_to_int  { static PyObject* convert(const OpenMPAccumulator<int>& acc){ return py::incref(PyLong_FromLong((long)acc.get())); } };
struct custom_OpenMPAccumulator_from_int{
	custom_OpenMPAccumulator_from_int(){  py::converter::registry::push_back(&convertible,&construct,py::type_id<OpenMPAccumulator<int> >()); }
	static void* convertible(PyObject* obj_ptr){ return PyLong_Check(obj_ptr) ? obj_ptr : 0; }
	static void construct(PyObject* obj_ptr, py::converter::rvalue_from_python_stage1_data* data){ void* storage=((py::converter::rvalue_from_python_storage<OpenMPAccumulator<int> >*)(data))->storage.bytes; new (storage) OpenMPAccumulator<int>; ((OpenMPAccumulator<int>*)storage)->set(py::extract<int>(obj_ptr)); data->convertible=storage; }
};

template<typename T>
struct custom_OpenMPArrayAccumulator_to_list {
	static PyObject* convert(const OpenMPArrayAccumulator<T>& acc){
		py::list ret; for(size_t i=0; i<acc.size(); i++) ret.append(acc.get(i));
		return py::incref(ret.ptr());
	}
};


struct custom_ptrMatchMaker_from_float{
	custom_ptrMatchMaker_from_float(){ py::converter::registry::push_back(&convertible,&construct,py::type_id<shared_ptr<MatchMaker> >()); }
	static void* convertible(PyObject* obj_ptr){ if(!PyNumber_Check(obj_ptr)) { cerr<<"Not convertible to MatchMaker"<<endl; return 0; } return obj_ptr; }
	static void construct(PyObject* obj_ptr, py::converter::rvalue_from_python_stage1_data* data){
		void* storage=((py::converter::rvalue_from_python_storage<shared_ptr<MatchMaker> >*)(data))->storage.bytes;
		new (storage) shared_ptr<MatchMaker>(new MatchMaker); // allocate the object at given address
		shared_ptr<MatchMaker>* mm=(shared_ptr<MatchMaker>*)(storage); // convert that address to our type
		(*mm)->algo="val"; (*mm)->val=PyFloat_AsDouble(obj_ptr); (*mm)->postLoad(**mm,NULL);
		data->convertible=storage;
	}
};

#if PY_MAJOR_VERSION < 3
struct custom_stdstring_from_unicode{
	custom_stdstring_from_unicode(){ py::converter::registry::push_back(&convertible,&construct,py::type_id<std::string>()); }
	static void* convertible(PyObject* obj_ptr){ if(PyUnicode_Check(obj_ptr)) return obj_ptr; return 0; }
	static void construct(PyObject* obj_ptr, py::converter::rvalue_from_python_stage1_data* data){
		PyObject* utf8=PyUnicode_AsUTF8String(obj_ptr); // encode into python string
		if(utf8==0) py::throw_error_already_set();
		char* str=PyString_AsString(utf8); // get NTBS from py::object
		void* storage=((py::converter::rvalue_from_python_storage<string>*)(data))->storage.bytes;
		new (storage) string(str); // allocate the std::string at given address
		py::decref(utf8); // buffer copy has been made on the previous line, decref to delete str
		data->convertible=storage;
	}
};
#endif



// inspired by
// https://www.maillist.unibas.ch/pipermail/openstructure-commits/Week-of-Mon-20100607/001773.html
template <typename Container, bool checkNone=true>
class SeqStrReprVisitor : public boost::python::def_visitor<SeqStrReprVisitor<Container>> {
	public:
	template <class Class>
	void visit(Class& cl) const {
	   cl.def("__str__", &__str__);
		cl.def("__repr__", &__str__);
	}
	private:
	static string __str__(Container& cl){
		std::ostringstream oss;
		oss<<"[";
 	   bool first=true;
		for (const auto& item: cl){
			if(first) first=false;
			else oss<<", ";
			if(checkNone && !item) oss<<"None";
			else oss<<item->pyStr();
	    }
   	oss<<"]";
		return oss.str();
	}
};



#if 0
template<typename numT, int dim>
struct custom_numpyBoost_to_py{
	static PyObject* convert(numpy_boost<numT, dim> nb){
		return nb.py_ptr(); // handles incref internally
	}
};
#endif

// this defines getstate and setstate methods to support pickling on linear sequences (should work for std::list as well)
template<class T>
struct VectorPickle: py::pickle_suite{
	static py::list getstate(const T& tt){ py::list ret; for(const typename T::value_type& t: tt) ret.append(t); return ret; }
	static void setstate(T& tt, py::list state){ tt.clear(); for(int i=0;i<py::len(state);i++) tt.push_back(py::extract<typename T::value_type>(state[i])); }
};

WOO_PYTHON_MODULE(_customConverters);
BOOST_PYTHON_MODULE(_customConverters){
	py::scope().attr("__name__")="woo._customConverters";

	if(getenv("WOO_DEBUG")) cerr<<"woo._customConverters: module initialization"<<endl;

	#if PY_MAJOR_VERSION < 3
		custom_stdstring_from_unicode();
	#endif

	custom_OpenMPAccumulator_from_float(); py::to_python_converter<OpenMPAccumulator<Real>, custom_OpenMPAccumulator_to_float>(); 
	custom_OpenMPAccumulator_from_int(); py::to_python_converter<OpenMPAccumulator<int>, custom_OpenMPAccumulator_to_int>(); 
	// to-python only
	py::to_python_converter<OpenMPAccumulator<Vector3r>,custom_OpenMPAccumulator_to_EigenMatrix<Vector3r>>();

	py::to_python_converter<OpenMPArrayAccumulator<int>, custom_OpenMPArrayAccumulator_to_list<int>>(); 
	py::to_python_converter<OpenMPArrayAccumulator<Real>, custom_OpenMPArrayAccumulator_to_list<Real>>(); 


	custom_ptrMatchMaker_from_float();

	// 2-way conversion for std::pair -- python 2-tuple
	#define PAIR_TUPLE_CONV(T) custom_CxxPair_from_PyTuple<T>(); py::to_python_converter<T,custom_CxxPair_to_PyTuple<T>>();
	typedef std::pair<int,string> pairIntString;
	typedef std::pair<string,Real> pairStringReal;
	//typedef vector<std::pair<string,Real>> vecPairStringReal;
	PAIR_TUPLE_CONV(pairIntString);
	PAIR_TUPLE_CONV(pairStringReal);

	// StrArrayMap (typedef for std::map<string,numpy_boost>) → python dictionary
	//custom_StrArrayMap_to_dict();
	// register from-python converter and to-python converter

	py::to_python_converter<vector<vector<string> >,custom_vvector_to_list<string> >();
	//py::to_python_converter<std::list<shared_ptr<Functor> >, custom_list_to_list<shared_ptr<Functor> > >();
	//py::to_python_converter<std::list<shared_ptr<Functor> >, custom_list_to_list<shared_ptr<Functor> > >();

	// this somehow did not work really...
	//py::to_python_converter<vector<py::object>,custom_vector_to_list<py::object>>();

	// don't return array of nodes as lists, each DemField.nodes[0] operation must create the list first,
	// pick the element, and throw it away; since node lists are typically long, create a custom class
	// using indexing suite (version 1; never found out how is the allegedly superior version 2
	// supposed to be used):
	// http://stackoverflow.com/questions/6157409/stdvector-to-boostpythonlist

	#define VECTOR_INDEXING_SUITE_EXPOSE(Type) py::class_<vector<shared_ptr<Type> > >(#Type "List").def(py::vector_indexing_suite<vector<shared_ptr<Type>>, /*NoProxy, shared_ptr provides proxy semantics already */true>()).def_pickle(VectorPickle<vector<shared_ptr<Type>>>()).def(SeqStrReprVisitor<vector<shared_ptr<Type>>>()); custom_vector_from_seq<shared_ptr<Type> >(); /* allow assignments vector<shared_ptr<Node> >=[list of nodes] */

	//	py::class_<vector<shared_ptr<Node> > >("NodeList").def(py::vector_indexing_suite<vector<shared_ptr<Node> >, /*NoProxy, shared_ptr provides proxy semantics already */true>()).def_pickle(VectorPickle<vector<shared_ptr<Node>>>());

	// register 2-way conversion between c++ vector and python homogeneous sequence (list/tuple) of corresponding type
		woo::converters_cxxVector_pyList_2way<int>();
		woo::converters_cxxVector_pyList_2way<size_t>();
		woo::converters_cxxVector_pyList_2way<bool>();
		woo::converters_cxxVector_pyList_2way<Real>();

		woo::converters_cxxVector_pyList_2way<Vector2r>();
		woo::converters_cxxVector_pyList_2way<Vector2i>();
		woo::converters_cxxVector_pyList_2way<Vector3r>();
		woo::converters_cxxVector_pyList_2way<Vector3i>();
		woo::converters_cxxVector_pyList_2way<Vector6r>();
		woo::converters_cxxVector_pyList_2way<Vector6i>();
		woo::converters_cxxVector_pyList_2way<VectorXr>();
		woo::converters_cxxVector_pyList_2way<Matrix3r>();
		woo::converters_cxxVector_pyList_2way<AlignedBox3r>();
		woo::converters_cxxVector_pyList_2way<AlignedBox2r>();
		woo::converters_cxxVector_pyList_2way<Quaternionr>();
		woo::converters_cxxVector_pyList_2way<string>();
		woo::converters_cxxVector_pyList_2way<std::pair<int,string>>();
		woo::converters_cxxVector_pyList_2way<std::pair<string,Real>>();
		woo::converters_cxxVector_pyList_2way<std::vector<std::pair<string,Real>>>();

		VECTOR_INDEXING_SUITE_EXPOSE(Node);
		VECTOR_INDEXING_SUITE_EXPOSE(Particle);
		VECTOR_INDEXING_SUITE_EXPOSE(Object);

	#undef VECTOR_INDEXING_SUITE_EXPOSE

	#if 0
		import_array();
		py::to_python_converter<numpy_boost<Real,1>, custom_numpyBoost_to_py<Real,1> >();
		py::to_python_converter<numpy_boost<Real,2>, custom_numpyBoost_to_py<Real,2> >();
		py::to_python_converter<numpy_boost<int,1>, custom_numpyBoost_to_py<int,1> >();
		py::to_python_converter<numpy_boost<int,2>, custom_numpyBoost_to_py<int,2> >();
	#endif

}




#endif /* WOO_PYBIND11 */
