// 2009 © Václav Šmilauer <eudoxos@arcig.cz>
#pragma once
#include<Python.h>
#include<string>
#ifdef WOO_PYBIND11
	typedef pybind11::gil_scoped_acquire GilLock;
#else
	//! class (scoped lock) managing python's Global Interpreter Lock (gil)
	class GilLock{
		PyGILState_STATE state;
		public:
			GilLock(){ state=PyGILState_Ensure(); }
			~GilLock(){ PyGILState_Release(state); }
	};
#endif
//! run string as python command; locks & unlocks GIL automatically
void pyRunString(const std::string& cmd);

