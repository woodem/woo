#include"except.hpp"
#include"gil.hpp"
#include"compat.hpp"


namespace woo{
	void StopIteration(){ PyErr_SetNone(PyExc_StopIteration); throw py::error_already_set(); }
	#define _DEFINE_WOO_PY_ERROR(x,y,AnyError) void AnyError(const std::string& what){ PyErr_SetString(BOOST_PP_CAT(PyExc_,AnyError),what.c_str()); throw py::error_already_set(); }
	BOOST_PP_SEQ_FOR_EACH(_DEFINE_WOO_PY_ERROR,~,WOO_PYUTIL_ERRORS)
	#undef _DEFINE_WOO_PY_ERROR

	// http://thejosephturner.com/blog/2011/06/15/embedding-python-in-c-applications-with-boostpython-part-2/
	std::string parsePythonException(py::error_already_set& e){
		py::gil_scoped_acquire lock;
		return parsePythonException_gilLocked(e);
	}

	// this the clean way of reporting exception
	// since https://github.com/pybind/pybind11/commit/35045eeef8969b7b446c64b192502ac1cbf7c451)
	// but not sure how to check it is supported nicely?
	std::string parsePythonException_gilLocked(py::error_already_set& e){
		std::string ret;
		if(!e.type() || !PyType_Check(e.type().ptr())){
			ret+="[type()=?]";
		}
		else ret+=((PyTypeObject*)e.type().ptr())->tp_name;
		ret+=": ";
		ret+=e.what();
		#ifdef WOO_NANOBIND
			auto traceback=e.traceback();
		#else
			auto traceback=e.trace();
		#endif
		if(!e.value()) ret+=" [value()=?]";
		else ret+="("+PY_CAST(string,py::str(e.value()))+")";
		if(!traceback) ret+=" [trace()=?]";
		else{
			py::object mod_tb(py::module_::import_("traceback"));
			py::object fmt_tb(mod_tb.attr("format_tb"));
			// Call format_tb to get a list of traceback strings
			py::object h_tb(traceback);
			py::object tb_list(fmt_tb(h_tb));
			// Extract the string, check the extraction, and fallback in necessary
			ret+="\n\n";
			try {
				for(auto s: tb_list) ret += PY_CAST(std::string,s);
			} catch(...) {
				ret += "[[error parsing traceback]]";
			}
		}
		return ret;
	}
};
