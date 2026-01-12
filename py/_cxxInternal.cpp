#include"../src/core/Master.hpp"
#include"../src/core/Timing.hpp"
#include"../src/supp/object/Object.hpp"
#include"../src/supp/base/Logging.hpp"
#include"../src/supp/base/openmp-accu.hpp"
#include"../src/supp/pyutil/compat.hpp"

#include"../src/lib/backward-cpp/backward.hpp"

#include"../src/supp/eigen/pybind11/register.hpp"

#include<signal.h>
#include<cstdlib>
#include<cstdio>
#include<iostream>
#include<string>
#include<stdexcept>

#include<boost/preprocessor/cat.hpp>
#include<boost/preprocessor/stringize.hpp>

#ifndef __MINGW64__
	#include<unistd.h>
	#include<termios.h>
	struct termios termios_attrs;
	bool termios_saved=false;
#endif

struct Boot{
	static std::shared_ptr<spdlog::logger> logger;

static void initSpdlog(){
	spdlog::set_pattern("%H:%M:%S [%-8n] %s:%# %^[%l] %v%$");
	if(!logger){ std::cerr<<"Logger not yet constructed...?"<<std::endl; return; }
	auto defaultLevel=(getenv("WOO_DEBUG")?spdlog::level::trace:spdlog::level::warn);
	spdlog::apply_all([&](std::shared_ptr<spdlog::logger> l){
		l->set_level(defaultLevel);
		l->flush_on(spdlog::level::err);
	});
	LOG_DEBUG("SpdLog initialized.");
};

#if defined(WOO_DEBUG) && !defined(__MINGW64__)
	static void crashHandler(int sig){
	switch(sig){
		case SIGABRT:
		case SIGSEGV:
			signal(SIGSEGV,SIG_DFL); signal(SIGABRT,SIG_DFL); // prevent loops - default handlers
			LOG_FATAL("SIGSEGV/SIGABRT handler called; gdb batch file is `{}'",Master::instance().gdbCrashBatch);
			int ret=std::system((string("gdb -x ")+Master::instance().gdbCrashBatch).c_str());
			if(ret) LOG_FATAL("Running the debugger failed (exit status {}); do you have gdb?",ret);
			raise(sig); // reemit signal after exiting gdb
			break;
		}
	}
#endif

#ifndef __MINGW64__
	static void quitHandler(int sig){
		if(sig!=SIGQUIT and sig!=SIGTERM and sig!=SIGINT) return;
		cerr<<"woo._cxxInternal: QUIT/TERM/INT handler called."<<endl;
		if(termios_saved){
			tcsetattr(STDIN_FILENO,TCSANOW,&termios_attrs);
			cerr<<"woo._cxxInternal: terminal cleared."<<endl;
		}
		// resend the signal
		signal(sig,SIG_DFL);
		raise(sig);
	}
#endif


/* Initialize woo - load config files, register python classes, set signal handlers */
static void wooInitialize(){

	initSpdlog();

	if(getenv("WOO_NO_BACKWARD")){ LOG_WARN("Backward stack trace disabled via WOO_NO_BACKWARD env var."); }
	else {
		backward::SignalHandling sh;
		if(sh.loaded()) { LOG_DEBUG("backward signal handlers installed."); }
		else LOG_WARN("backward signal handlers not installed correctly (proceeding).");
	}
	
	// this is called automatically since python3.7, and deprecation warning is produced since 3.9
	#if PY_VERSION_HEX<0x03070000
		PyEval_InitThreads();
	#endif

	#if PYBIND11_VERSION_HEX>=0x02060000
		py::module_ m=py::module_::create_extension_module(
	#else
		py::module m(
	#endif
		"_wooEigen11","Woo's internal wrapper of some Eigen classes; most generic classes are exposed via pybind11's numpy interface as numpy arrays, only special-need cases are wrapped here."
		#if PYBIND11_VERSION_HEX>=0x02060000
			 ,new py::module_::module_def
		#endif
	);
	woo::registerEigenClassesInPybind11(m);
	woo::registerOpenMPAccuClassesInPybind11(m);
	auto sysModules=py::extract<py::dict>(py::getattr(py::import("sys"),"modules"))();
	if(sysModules.contains("minieigen")) LOG_FATAL("sys.modules['minieigen'] is already there, expect trouble (this build uses pybind11-based internal wrapper of eigen, not boost::python-based minieigen");
	sysModules["_wooEigen11"]=m;
	sysModules["minieigen"]=m;
	LOG_DEBUG_EARLY("sys.modules['minieigen'] is alias for _wooEigen11.")

	Master& master(Master::instance());

	string confDir;
	if(getenv("XDG_CONFIG_HOME")){
		confDir=getenv("XDG_CONFIG_HOME");
	} else {
		#ifndef __MINGW64__ // POSIX
			if(getenv("HOME")) confDir=string(getenv("HOME"))+"/.config";
		#else
			// windows stuff; not tested
			// http://stackoverflow.com/a/4891126/761090
			if(getenv("USERPROFILE")) confDir=getenv("USERPROFILE");
			else if(getenv("HOMEDRIVE") && getenv("HOMEPATH")) confDir=string(getenv("HOMEDRIVE"))+string(getenv("HOMEPATH"));
		#endif
		else LOG_WARN("Unable to determine home directory; no user-configuration will be loaded.");
	}

	confDir+="/woo";

	master.confDir=confDir;
	#if defined(WOO_DEBUG) && !defined(__MINGW64__)
		std::ofstream gdbBatch;
		master.gdbCrashBatch=master.tmpFilename();
		gdbBatch.open(master.gdbCrashBatch.c_str()); gdbBatch<<"attach "<<to_string(getpid())<<"\nset pagination off\nthread info\nthread apply all backtrace\ndetach\nquit\n"; gdbBatch.close();
		// XXX DISABLED for now
		#if 0
			signal(SIGABRT,&Boot::crashHandler);
			signal(SIGSEGV,&Boot::crashHandler);
		#endif
	#endif

	#ifndef __MINGW64__ // posix
		if(getenv("TERM")){
			tcgetattr(STDIN_FILENO,&termios_attrs);
			termios_saved=true;
			signal(SIGQUIT,&Boot::quitHandler);
			signal(SIGTERM,&Boot::quitHandler);
			signal(SIGINT,&Boot::quitHandler);
			// cerr<<"woo._cxxInternal: QUIT/TERM/INT handler registered."<<endl;
		}
	#endif

	// check that the decimal separator is "." (for GTS imports)
	if(atof("0.5")==0.0){
		LOG_WARN("Decimal separator is not '.'; this can cause erratic mesh imports from GTS and perhaps other problems. Please report this to https://github.com/woodem/woo .");
	}
	// register all python classes here
	master.pyRegisterAllClasses();
}

};

std::shared_ptr<spdlog::logger> Boot::logger=spdlog::stdout_color_mt("woo.boot");

#ifdef WOO_GTS
	// this module is compiled from separate sources (in py/3rd-party/pygts)
	// but we will register it here
	WOO_PYTHON_MODULE(_gts);
#endif

// NB: this module does NOT use WOO_PYTHON_MODULE, since the file is really called _cxxInternal[_flavor].so
// and is a real real python module

	PYBIND11_MODULE(BOOST_PP_CAT(_cxxInternal,WOO_CXX_FLAVOR),mod){
		LOG_DEBUG_EARLY("Initializing the _cxxInternal" BOOST_PP_STRINGIZE(WOO_CXX_FLAVOR) " module.");
		mod.doc()="This module's binary contains all compiled Woo modules (such as :obj:`woo.core`), which are created dynamically when this module is imported for the first time. In itself, it is empty and only to be used internally.";

		Boot::wooInitialize();
	};
