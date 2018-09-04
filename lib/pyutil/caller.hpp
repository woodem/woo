#pragma once
// http://stackoverflow.com/questions/16933418/calling-c-functions-who-is-calling
#include<frameobject.h>
#include<woo/lib/pyutil/gil.hpp>

namespace woo {
	static bool pyCallerInfo(string& file, string& func, int& line){
		GilLock l; // auto-desctructed at the end of scope
		PyFrameObject* frame=PyEval_GetFrame(); if(!frame || !frame->f_code) return false;
		#ifdef WOO_PYBIND11
			file=py::cast<string>(frame->f_code->co_filename);
			func=py::cast<string>(frame->f_code->co_name);
		#else
			file=py::extract<string>(frame->f_code->co_filename);
			func=py::extract<string>(frame->f_code->co_name);
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
