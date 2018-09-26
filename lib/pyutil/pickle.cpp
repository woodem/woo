#include<woo/lib/pyutil/pickle.hpp>
#include<woo/lib/pyutil/gil.hpp>
#include<woo/lib/pyutil/compat.hpp>

namespace woo{
	py::object Pickler::cPickle_dumps;
	py::object Pickler::cPickle_loads;
	bool Pickler::initialized=false;

	void Pickler::ensureInitialized(){
		if(initialized) return;
		GilLock pyLock;
		//cerr<<"[Pickler::ensureInitialized:gil]";
		#ifndef WOO_PYBIND11
			#if PY_MAJOR_VERSION >= 3
				py::object cPickle=py::import("pickle");
			#else
				py::object cPickle=py::import("cPickle");
			#endif
		#else
			py::object cPickle=py::module::import("pickle");
		#endif
		cPickle_dumps=cPickle.attr("dumps");
		cPickle_loads=cPickle.attr("loads");
		initialized=true;
	}

	std::string Pickler::dumps(py::object o){
		ensureInitialized();
		GilLock pyLock;
		#ifdef WOO_PYBIND11
			py::object minus1=py::cast(-1);
			// watch out! the object might have NULL pointer (uninitialized??) so make sure to pass None in that case
			PyObject *b=PyObject_CallFunctionObjArgs(cPickle_dumps.ptr(),o.ptr()?o.ptr():Py_None,minus1.ptr(),NULL);
			if(!b){
				if(PyErr_Occurred()) throw py::error_already_set();
				else throw std::runtime_error("Pickling failed but no python erro was set??");
			}
			assert(PyBytes_Check(b));
			return std::string(py::reinterpret_steal<py::bytes>(b));
		#else
			#if (PY_MAJOR_VERSION >= 3)
				// destructed at the end of the scope, when std::string copied the content already
				py::object s(cPickle_dumps(o,-1)); // -1: use binary protocol
				PyObject* b=s.ptr();
				assert(PyBytes_Check(b));
				//cerr<<"[dumps:gil:length="<<PyBytes_Size(b)<<"]";
				// bytes may contain 0 (only in py3k apparently), make sure size is passed explicitly
				return std::string(PyBytes_AsString(b),PyBytes_Size(b)); 
			#else
				return py::extract<string>(cPickle_dumps(o,-1))(); // -1: use binary protocol
			#endif
		#endif
		;
	}
	py::object Pickler::loads(const std::string& s){
		ensureInitialized();
		GilLock pyLock;
		//cerr<<"[loads:gil]";
		#ifndef WOO_PYBIND11
			#if PY_MAJOR_VERSION >= 3
				return cPickle_loads(py::handle<>(PyBytes_FromStringAndSize(s.data(),(Py_ssize_t)s.size())));
			#else
				return cPickle_loads(s);
			#endif
		#else
			// return cPickle_loads(py::reinterpret_borrow<py::object>(PyBytes_FromStringAndSize(s.data(),(Py_ssize_t)s.size())));
			return cPickle_loads(py::handle(PyBytes_FromStringAndSize(s.data(),(Py_ssize_t)s.size())));
		#endif
	}
}
