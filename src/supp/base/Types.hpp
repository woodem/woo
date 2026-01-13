#pragma once

#ifdef WOO_DEBUG
	#define WOO_CAST dynamic_cast
	#define WOO_PTR_CAST dynamic_pointer_cast
#else
	#define WOO_CAST static_cast
	#define WOO_PTR_CAST static_pointer_cast
#endif

// prevent VTK from #including strstream which gives warning about deprecated header
// this is documented in vtkIOStream.h
#define VTK_EXCLUDE_STRSTREAM_HEADERS


#ifdef __clang__
	// for missing override specification in place where we cannot change it (Q_OBJECT, for instance)
	#define WOO_NOWARN_OVERRIDE_PUSH _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Winconsistent-missing-override\"")
	#define WOO_NOWARN_OVERRIDE_POP _Pragma("GCC diagnostic pop")
#else
	#define WOO_NOWARN_OVERRIDE_PUSH
	#define WOO_NOWARN_OVERRIDE_POP
#endif

#include<initializer_list>
#include<memory>

using std::shared_ptr;
using std::enable_shared_from_this;
using std::static_pointer_cast;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::weak_ptr;

#include<vector>
#include<map>
#include<set>
#include<list>
#include<string>
#include<iostream>
#include<stdexcept>
#include<tuple>
#include<cmath>
#include<sstream>
using std::unique_ptr;
using std::vector;
using std::map;
using std::set;
using std::list;
using std::string;
using std::cerr;
using std::cout;
using std::endl;
using std::exception;
using std::pair;
using std::make_pair;
using std::runtime_error;
using std::invalid_argument;
using std::logic_error;
using std::max;
using std::min;
using std::abs;
using std::isnan;
using std::isinf;
using std::to_string;
template<typename T>
std::string ptr_to_string(T* p){ std::ostringstream oss; oss<<p; return oss.str(); }

#include<filesystem>

#define PYBIND11_DETAILED_ERROR_MESSAGES
// includes python headers, which also define PY_MAJOR_VERSION
#include<pybind11/pybind11.h>
#include<pybind11/eval.h>
#include<pybind11/operators.h>
#include<pybind11/stl.h>
// #include<pybind11/eigen.h>
#include"../pyutil/compat.hpp"
namespace py=pybind11;

typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned short ushort;


