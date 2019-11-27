#pragma once

// replace deprecated BOOST_FOREACH by range-for
#define FOREACH(a,b) for(a:b)

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

#if 0
	// for internal woo::Object macros which do funny things internally
	#define WOO_NOWARN_UNEVALUATED_PUSH  _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wunevaluated-expression\"")
	#define WOO_NOWARN_UNEVALUATED_POP
#endif




#include<boost/lexical_cast.hpp>
using boost::lexical_cast;

// enables beautiful constructs like: for(int x: {0,1,2})...
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

using std::unique_ptr;

#if 0
	#include<memory>
	using std::shared_ptr;
	namespace boost { namespace python { template<class T> T* get_pointer(std::shared_ptr<T> const& p){ return p.get(); } } }
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
#ifndef WOO_WORKAROUND_CXX11_MATH_DECL_CONFLICT
		// this would trigger bugs under Linux (math.h is included somewhere behind the scenes?)
		// see  http://gcc.gnu.org/bugzilla/show_bug.cgi?id=48891
		using std::isnan;
		using std::isinf;
#endif

// workaround for http://gcc.gnu.org/bugzilla/show_bug.cgi?id=52015
// "std::to_string does not work under MinGW"
// reason: gcc built without --enable-c99 does not support to_string (checked with 4.7.1)
// so we just emulate that thing with lexical_cast
#if defined(__MINGW64__) || defined(__MINGW32__)
	template<typename T> string to_string(const T& t){ return lexical_cast<string>(t); }
#else
	using std::to_string;
#endif

// override keyword not supported until gcc 4.7
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ == 4 && __GNUC_MINOR__ < 7
	// #define override
	#error GCC<=4.6 is no longer supported
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
	// this should not be enabled yet
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


