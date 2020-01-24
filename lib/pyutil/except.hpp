#pragma once

#include<string>
#include<boost/preprocessor.hpp>
#include<boost/format.hpp>

#ifdef WOO_PYBIND11
	#include<pybind11/pybind11.h>
	namespace py=pybind11;
#else
	#include<boost/python.hpp>
	namespace py=boost::python;
#endif

namespace woo{
	void StopIteration();
	#define WOO_PYUTIL_ERRORS (ArithmeticError)(AttributeError)(IndexError)(KeyError)(NameError)(NotImplementedError)(RuntimeError)(TypeError)(ValueError)
	#define _DECLARE_WOO_PY_ERROR(x,y,err) void err(const std::string&); void err(const boost::format& f);
	BOOST_PP_SEQ_FOR_EACH(_DECLARE_WOO_PY_ERROR,~,WOO_PYUTIL_ERRORS)
	#undef _DECLARE_WOO_PY_ERROR

	// return string representation of current python exception
	std::string parsePythonException(py::error_already_set& e);
	// use this if the GIL is already locked
	std::string parsePythonException_gilLocked(py::error_already_set& e);
};
