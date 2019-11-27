#include<woo/lib/pyutil/except.hpp>
#include<woo/lib/pyutil/gil.hpp>
#ifndef WOO_PYBIND11
	#include<boost/python.hpp>
	namespace py=boost::python;
#else
	#include<pybind11/pybind11.h>
	namespace py=pybind11;
#endif

namespace woo{
	void StopIteration(){ PyErr_SetNone(PyExc_StopIteration); throw py::error_already_set(); }
	#define _DEFINE_WOO_PY_ERROR(x,y,AnyError) void AnyError(const std::string& what){ PyErr_SetString(BOOST_PP_CAT(PyExc_,AnyError),what.c_str()); throw py::error_already_set(); } void AnyError(const boost::format& f){ AnyError(f.str()); }
	// void ArithmeticError(const std::string& what){ PyErr_SetString(PyExc_ArithmeticError,what.c_str()); boost::python::throw_error_already_set(); }
	BOOST_PP_SEQ_FOR_EACH(_DEFINE_WOO_PY_ERROR,~,WOO_PYUTIL_ERRORS)
	#undef _DEFINE_WOO_PY_ERROR
	
	// http://thejosephturner.com/blog/2011/06/15/embedding-python-in-c-applications-with-boostpython-part-2/
	std::string parsePythonException(){
		GilLock lock;
		return parsePythonException_gilLocked();
	}
	std::string parsePythonException_gilLocked(){
		PyObject *type_ptr = NULL, *value_ptr = NULL, *traceback_ptr = NULL;
		PyErr_Fetch(&type_ptr, &value_ptr, &traceback_ptr);
		std::string ret("Unfetchable Python error: ");
		#ifdef WOO_PYBIND11
			if(type_ptr != NULL) {
				pybind11::handle h_type(type_ptr);
				pybind11::str    type_pstr(h_type);
				try { ret = type_pstr.cast<std::string>(); }
				catch(const pybind11::value_error& e) { ret = "Unknown exception type"; }
			}
			if(value_ptr) {
				pybind11::handle h_val(value_ptr);
				pybind11::str    a(h_val);
				try { ret += a.cast<std::string>(); }
				catch(const pybind11::value_error &e) { ret += ": Unparseable Python error: "; }
			}
			// Parse lines from the traceback using the Python traceback module
			if(traceback_ptr) {
				pybind11::handle h_tb(traceback_ptr);
				// Load the traceback module and the format_tb function
				pybind11::object tb(pybind11::module::import("traceback"));
				pybind11::object fmt_tb(tb.attr("format_tb"));
				// Call format_tb to get a list of traceback strings
				pybind11::object tb_list(fmt_tb(h_tb));
				// Extract the string, check the extraction, and fallback in necessary
				try {
					for(auto &s: tb_list) ret += s.cast<std::string>();
				} catch(const pybind11::value_error &e) {
					ret += ": Unparseable Python traceback";
				}
			}
		#else
			if(type_ptr != NULL){
				py::handle<> h_type(type_ptr);
				py::str type_pstr(h_type);
				py::extract<std::string> e_type_pstr(type_pstr);
				if(e_type_pstr.check())
					ret = e_type_pstr();
				else
					ret = "Unknown exception type";
			}
			if(value_ptr != NULL){
				py::handle<> h_val(value_ptr);
				py::str a(h_val);
				py::extract<std::string> returned(a);
				if(returned.check())
					ret +=  ": " + returned();
				else
					ret += std::string(": Unparseable Python error: ");
			}
			if(traceback_ptr != NULL){
				py::handle<> h_tb(traceback_ptr);
				py::object tb(py::import("traceback"));
				py::object fmt_tb(tb.attr("format_tb"));
				py::object tb_list(fmt_tb(h_tb));
				py::object tb_str(py::str("\n").join(tb_list));
				py::extract<std::string> returned(tb_str);
				if(returned.check())
					ret += ": " + returned();
				else
					ret += std::string(": Unparseable Python traceback");
			}
		#endif
		return ret;
	}

};
