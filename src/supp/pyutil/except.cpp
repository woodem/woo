#include"except.hpp"
#include"gil.hpp"


namespace woo{
	void StopIteration(){ PyErr_SetNone(PyExc_StopIteration); throw py::error_already_set(); }
	#define _DEFINE_WOO_PY_ERROR(x,y,AnyError) void AnyError(const std::string& what){ PyErr_SetString(BOOST_PP_CAT(PyExc_,AnyError),what.c_str()); throw py::error_already_set(); }
	BOOST_PP_SEQ_FOR_EACH(_DEFINE_WOO_PY_ERROR,~,WOO_PYUTIL_ERRORS)
	#undef _DEFINE_WOO_PY_ERROR

	// http://thejosephturner.com/blog/2011/06/15/embedding-python-in-c-applications-with-boostpython-part-2/
	std::string parsePythonException(py::error_already_set& e){
		GilLock lock;
		return parsePythonException_gilLocked(e);
	}

	// this the clean way of reporting exception
	// since https://github.com/pybind/pybind11/commit/35045eeef8969b7b446c64b192502ac1cbf7c451)
	// but not sure how to check it is supported nicely?
	std::string parsePythonException_gilLocked(py::error_already_set& e){
		if(!e.type() || !PyType_Check(e.type().ptr())) return "unreported exception type";
		if(!e.value()) return "unreported exception value";
		if(!e.trace()) return "unreported exception traceback";
		std::string ret=((PyTypeObject*)e.type().ptr())->tp_name;
		ret+=": ";
		ret+=py::str(e.value()).cast<std::string>();
		pybind11::object mod_tb(pybind11::module::import("traceback"));
		pybind11::object fmt_tb(mod_tb.attr("format_tb"));
		// Call format_tb to get a list of traceback strings
		pybind11::object h_tb(e.trace());
		pybind11::object tb_list(fmt_tb(h_tb));
		// Extract the string, check the extraction, and fallback in necessary
		ret+="\n\n";
		try {
			for(auto &s: tb_list) ret += s.cast<std::string>();
		} catch(const pybind11::value_error &e) {
			ret += "[[error parsing traceback]]";
		}
		return ret;
	}
};
