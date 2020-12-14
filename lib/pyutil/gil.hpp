// 2009 © Václav Šmilauer <eudoxos@arcig.cz>
#pragma once
#include<Python.h>
#include<string>
#include<pybind11/pybind11.h>
typedef pybind11::gil_scoped_acquire GilLock;
//! run string as python command; locks & unlocks GIL automatically
void pyRunString(const std::string& cmd);

