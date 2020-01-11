#pragma once
#ifdef WOO_PYBIND11
#include<pybind11/pybind11.h>
namespace woo{
	void registerEigenClassesInPybind11(py::module& mod);
};
#endif
