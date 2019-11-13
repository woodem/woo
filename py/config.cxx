// this file has the .cxx extension since we compile it separately, not
// matching the py/*.cpp glob in the build system
#include<woo/core/Master.hpp>
#include<boost/algorithm/string/predicate.hpp>
#include<boost/preprocessor.hpp>

static const string version(BOOST_PP_STRINGIZE(WOO_VERSION));
static const string revision(BOOST_PP_STRINGIZE(WOO_REVISION));

static string prettyVersion(bool lead=true){
	if(!version.empty()) return (lead?"ver. ":"")+version;
	if(boost::starts_with(revision,"bzr")) return (lead?"rev. ":"")+revision.substr(3);
	return version+"/"+revision;
}

WOO_PYTHON_MODULE(config)
#ifdef WOO_PYBIND11
	PYBIND11_MODULE(config,mod){
#else
	BOOST_PYTHON_MODULE(config){
#endif
	py::list features;
		#ifdef WOO_LOG4CXX
			features.append("log4cxx");
		#endif
		#ifdef WOO_OPENMP
			features.append("openmp");
		#endif
		#ifdef WOO_OPENGL
			features.append("opengl");
		#endif
		#ifdef WOO_CGAL
			features.append("cgal");
		#endif
		#ifdef WOO_OPENCL
			features.append("opencl");
		#endif
		#ifdef WOO_GTS
			features.append("gts");
		#endif
		#ifdef WOO_VTK
			features.append("vtk");
		#endif
		#ifdef WOO_GL2PS
			features.append("gl2ps");
		#endif
		#ifdef WOO_QT4
			features.append("qt4");
			features.append("qt");
		#endif
		#ifdef WOO_QT5
			features.append("qt5");
			features.append("qt");
		#endif
		#ifdef WOO_CLDEM
			features.append("cldem");
		#endif
		#ifdef WOO_NOXML
			features.append("noxml");
		#endif
		#ifdef WOO_ALIGN
			features.append("align");
		#endif
		#ifdef WOO_HDF5
			features.append("hdf5");
		#endif
		#ifdef WOO_PYBIND11
			features.append("pybind11");
		#endif
		#ifdef WOO_SPDLOG
			features.append("spdlog");
		#endif

	#ifndef WOO_PYBIND11
		auto mod=py::scope();
	#endif

	mod.attr("features")=features;

	mod.attr("debug")=
	#ifdef WOO_DEBUG
		true;
	#else
		false;
	#endif

	#if defined(__clang__)
		mod.attr("compiler")=py::make_tuple("clang",__clang_version__);
	#elif defined(__INTEL_COMPILER)
		mod.attr("compiler")=py::make_tuple("icc",BOOST_PP_STRINGIZE(__INTEL_COMPILER));
	#elif defined(__GNUC__) /* must come after clang and icc, which also define __GNUC__ */
		mod.attr("compiler")=py::make_tuple("gcc", BOOST_PP_STRINGIZE(__GNUC__) "." BOOST_PP_STRINGIZE(__GNUC_MINOR__) "." BOOST_PP_STRINGIZE(__GNUC_PATCHLEVEL__));
	#else
		mod.attr("compiler")=py::make_tuple("unknown (not gcc,clang,icc)","unknown")
	#endif
		
	mod.attr("prefix")=BOOST_PP_STRINGIZE(WOO_PREFIX);
	mod.attr("suffix")=BOOST_PP_STRINGIZE(WOO_SUFFIX);

	mod.attr("revision")=BOOST_PP_STRINGIZE(WOO_REVISION);
	mod.attr("version")=BOOST_PP_STRINGIZE(WOO_VERSION);
	#ifdef WOO_PYBIND11
		mod.
	#else
	py::
	#endif
		def("prettyVersion",&prettyVersion,(py::arg("lead")=true));

	mod.attr("sourceRoot")=BOOST_PP_STRINGIZE(WOO_SOURCE_ROOT);
	mod.attr("buildRoot")=BOOST_PP_STRINGIZE(WOO_BUILD_ROOT);
	mod.attr("flavor")=BOOST_PP_STRINGIZE(WOO_FLAVOR);
	#ifdef WOO_SCONS_PATH
		mod.attr("sconsPath")=BOOST_PP_STRINGIZE(WOO_SCONS_PATH);
	#endif
	mod.attr("buildDate")=__DATE__;

};
