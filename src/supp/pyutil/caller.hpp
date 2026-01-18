#pragma once
// http://stackoverflow.com/questions/16933418/calling-c-functions-who-is-calling
#include<frameobject.h>
#include"gil.hpp"

namespace woo {
	static bool pyCallerInfo(string& file, string& func, int& line){
		py::gil_scoped_acquire l; // auto-desctructed at the end of scope
		PyFrameObject* frame=PyEval_GetFrame(); 
		if(!frame) return false;
		//  python >= 3.12
		#if PY_VERSION_HEX >= 0x030c0000
			PyCodeObject* code=PyFrame_GetCode(frame);
		#else
			PyCodeObject* code=frame->f_code;
		#endif
		if(!code) return false;
		#ifdef WOO_NANOBIND
			file=py::steal<py::str>(code->co_filename).c_str();
			func=py::steal<py::str>(code->co_name).c_str();
		#else
			file=py::cast<string>(code->co_filename);
			func=py::cast<string>(code->co_name);
		#endif
		line=PyFrame_GetLineNumber(frame);
		return true;
	}
	static string pyCallerInfo(){
		string fi,fu; int li;
		if(pyCallerInfo(fi,fu,li)) return fi+":"+to_string(li)+" in "+fu;
		return "<unable to determine caller location>";
	}
};
