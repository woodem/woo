#pragma once
#define PYBIND11_DETAILED_ERROR_MESSAGES
#include<pybind11/pybind11.h>
namespace woo{
	void registerEigenClassesInPybind11(py::module& mod);
};
