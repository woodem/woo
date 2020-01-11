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

#if defined(WOO_CEREAL) or defined(WOO_PYBIND11)
	#define WOO_STD_SHAREDPTR
#endif

#ifdef WOO_STD_SHAREDPTR
	using std::shared_ptr;
	using std::enable_shared_from_this;
	using std::static_pointer_cast;
	using std::dynamic_pointer_cast;
	using std::make_shared;
	using std::weak_ptr;
	// boost::python with std::shared_ptr
	#ifndef WOO_PYBIND11
		#define WOO_PY_DERIVED_BASE_SHAREDPTR_CONVERTIBLE(Derived,Base) boost::python::implicitly_convertible<std::shared_ptr<Derived>,std::shared_ptr<Base>>();
	#endif
#else
	/* Avoid using std::shared_ptr, because boost::serialization does not support it.
	   For boost::python, declaring get_pointer template would be enough.
		See http://stackoverflow.com/questions/6568952/c0x-stdshared-ptr-vs-boostshared-ptr
	*/
	#include<boost/shared_ptr.hpp>
	#include<boost/make_shared.hpp>
	#include<boost/enable_shared_from_this.hpp>
	using boost::shared_ptr;
	using boost::enable_shared_from_this;
	using boost::static_pointer_cast;
	using boost::dynamic_pointer_cast;
	using boost::make_shared;
	using boost::weak_ptr;
	// not needed here
	#define WOO_PY_DERIVED_BASE_SHAREDPTR_CONVERTIBLE(Derived,Base)
#endif



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

#if __has_include(<filesystem>)
	#include<filesystem>
	namespace filesystem=std::filesystem;
#elif __has_include(<experimental/filesystem>)
	#include<experimental/filesystem>
	namespace filesystem=std::experimental::filesystem;
#else
	#error Neither <filesystem> nor <experimental/filesystem> are includable!
#endif

// includes python headers, which also define PY_MAJOR_VERSION
#ifndef WOO_PYBIND11
	#include<boost/python.hpp>
	#include<woo/lib/pyutil/compat.hpp>
	namespace py=boost::python;
#else
	#include<pybind11/pybind11.h>
	#include<pybind11/eval.h>
	#include<pybind11/operators.h>
	#include<pybind11/stl.h>
	// #include<pybind11/eigen.h>
	#include<woo/lib/pyutil/compat.hpp>
	namespace py=pybind11;
#endif

// py 2x: iterator.next, py3k: iterator.__next__
#if PY_MAJOR_VERSION >= 3
	#define WOO_next_OR__next__ "__next__"
#else
	#define WOO_next_OR__next__ "next"
#endif

typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned short ushort;


