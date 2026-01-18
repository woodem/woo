#include"pickle.hpp"
#include"gil.hpp"
#include"compat.hpp"

namespace woo{
	py::handle Pickler::cPickle_dumps;
	py::handle Pickler::cPickle_loads;
	bool Pickler::initialized=false;

	void Pickler::ensureInitialized(){
		if(initialized) return;
		py::gil_scoped_acquire pyLock;
		//cerr<<"[Pickler::ensureInitialized:gil]";
		py::object cPickle=py::module_::import_("pickle");
		cPickle_dumps=cPickle.attr("dumps");
		cPickle_loads=cPickle.attr("loads");
		initialized=true;
	}

	std::string Pickler::dumps(py::object o){
		ensureInitialized();
		py::gil_scoped_acquire pyLock;
		py::object minus1=py::cast(-1);
		// watch out! the object might have NULL pointer (uninitialized??) so make sure to pass None in that case
		PyObject *b=PyObject_CallFunctionObjArgs(cPickle_dumps.ptr(),o.ptr()?o.ptr():Py_None,minus1.ptr(),NULL);
		if(!b){
			if(PyErr_Occurred()) throw py::error_already_set();
			else throw std::runtime_error("Pickling failed but no python erro was set??");
		}
		assert(PyBytes_Check(b));
		#ifdef WOO_NANOBIND
			return py::steal<py::bytes>(b).c_str();
		#else
			return std::string(py::reinterpret_steal<py::bytes>(b));
		#endif
	}
	py::object Pickler::loads(const std::string& s){
		ensureInitialized();
		py::gil_scoped_acquire pyLock;
		//cerr<<"[loads:gil]";
		return cPickle_loads(py::handle(PyBytes_FromStringAndSize(s.data(),(Py_ssize_t)s.size())));
	}
}
