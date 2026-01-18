#pragma once

#include"compat.hpp"
#include<string>
#include<format>
#include<boost/preprocessor.hpp>
#include"../base/Logging.hpp" // for fmt (possibly bundled with spdlog)


namespace woo{
	void StopIteration();
	#define WOO_PYUTIL_ERRORS (ArithmeticError)(AttributeError)(IndexError)(KeyError)(NameError)(NotImplementedError)(RuntimeError)(TypeError)(ValueError)
	#define _DECLARE_WOO_PY_ERROR(x,y,err) void err(const std::string&); \
		template<typename... Args> void err(fmt::format_string<Args...> s, Args&&... args){ err(fmt::format(s,std::forward<Args>(args)...)); }
	BOOST_PP_SEQ_FOR_EACH(_DECLARE_WOO_PY_ERROR,~,WOO_PYUTIL_ERRORS)
	#undef _DECLARE_WOO_PY_ERROR

	// return string representation of current python exception
	std::string parsePythonException(py::error_already_set& e);
	// use this if the GIL is already locked
	std::string parsePythonException_gilLocked(py::error_already_set& e);
};
