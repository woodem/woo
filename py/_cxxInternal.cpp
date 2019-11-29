#include<woo/core/Master.hpp>
#include<woo/core/Timing.hpp>
#include<woo/lib/object/Object.hpp>
#include<woo/lib/base/Logging.hpp>

#include<signal.h>
#include<cstdlib>
#include<cstdio>
#include<iostream>
#include<string>
#include<stdexcept>

#include<boost/filesystem/convenience.hpp>
#include<boost/preprocessor/cat.hpp>
#include<boost/preprocessor/stringize.hpp>

#ifndef __MINGW64__
	#include<unistd.h>
	#include<termios.h>
	struct termios termios_attrs;
	bool termios_saved=false;
#endif


#ifdef WOO_SPDLOG
	static std::shared_ptr<spdlog::logger> logger=spdlog::stdout_color_mt("woo.boot");
	void initSpdlog(){
		spdlog::set_pattern("%H:%M:%S [%-8n] %s:%# %^[%l] %v%$");
		if(!logger){ std::cerr<<"Logger not yet constructed...?"<<std::endl; return; }
		auto defaultLevel=(getenv("WOO_DEBUG")?spdlog::level::debug:spdlog::level::warn);
		spdlog::apply_all([&](std::shared_ptr<spdlog::logger> l){
			l->set_level(defaultLevel);
			l->flush_on(spdlog::level::err);
		});
		LOG_DEBUG("SpdLog initialized.");
	};
#endif

#if defined(WOO_DEBUG) && !defined(__MINGW64__)
	void crashHandler(int sig){
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

#ifndef __MING64__
	void quitHandler(int sig){
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
void wooInitialize(){

	#ifdef WOO_SPDLOG
		initSpdlog();
	#endif

	PyEval_InitThreads();

	// this is probably too late already
	#if 0
		#ifdef WOO_PYBIND11
			py::module::import("_minieigen11");
		#else
			py::import("minieigen"); 
		#endif
	#else
		// early check that minieigen is importable
		#ifdef WOO_PYBIND11
			const string meig="_minieigen11";
		#else
			const string meig="minieigen"; 
		#endif
		try{
			if(getenv("WOO_DEBUG")) LOG_DEBUG_EARLY("Attemting "<<meig<<" import...");
			#ifdef WOO_PYBIND11
				auto minieigen=py::module::import(meig.c_str());
			#else
				auto minieigen=py::import(meig.c_str());
			#endif
			LOG_DEBUG_EARLY(meig<<" module @ "<<minieigen.ptr());
		} catch(py::error_already_set& e){
				throw std::runtime_error("Error importing "+meig+":\n"+parsePythonException_gilLocked());
		} catch(...){
			throw std::runtime_error("Error importing "+meig+" (details not reported).");
		}
	#endif



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
		gdbBatch.open(master.gdbCrashBatch.c_str()); gdbBatch<<"attach "<<lexical_cast<string>(getpid())<<"\nset pagination off\nthread info\nthread apply all backtrace\ndetach\nquit\n"; gdbBatch.close();
		// XXX DISABLED for now
		#if 0
			signal(SIGABRT,crashHandler);
			signal(SIGSEGV,crashHandler);
		#endif
	#endif
	
	#ifndef __MINGW64__ // posix
		if(getenv("TERM")){
			tcgetattr(STDIN_FILENO,&termios_attrs);
			termios_saved=true;
			signal(SIGQUIT,quitHandler);
			signal(SIGTERM,quitHandler);
			signal(SIGINT,quitHandler);
			cerr<<"woo._cxxInternal: QUIT/TERM/INT handler registered."<<endl;
		}
	#endif

	// check that the decimal separator is "." (for GTS imports)
	if(atof("0.5")==0.0){
		LOG_WARN("Decimal separator is not '.'; this can cause erratic mesh imports from GTS and perhaps other problems. Please report this to https://github.com/woodem/woo .");
	}
	// register all python classes here
	master.pyRegisterAllClasses();
}

#ifdef WOO_GTS
	// this module is compiled from separate sources (in py/3rd-party/pygts)
	// but we will register it here
	WOO_PYTHON_MODULE(_gts);
#endif

// NB: this module does NOT use WOO_PYTHON_MODULE, since the file is really called _cxxInternal[_flavor][_debug].so
// and is a real real python module
// 

#ifdef WOO_PYBIND11
	#ifdef WOO_DEBUG
		PYBIND11_MODULE(BOOST_PP_CAT(BOOST_PP_CAT(_cxxInternal,WOO_CXX_FLAVOR),_debug),mod){
	#else
		PYBIND11_MODULE(BOOST_PP_CAT(_cxxInternal,WOO_CXX_FLAVOR),mod){
	#endif
		LOG_DEBUG_EARLY("Initializing the _cxxInternal" BOOST_PP_STRINGIZE(WOO_CXX_FLAVOR) " module.");
		mod.doc()="This module's binary contains all compiled Woo modules (such as :obj:`woo.core`), which are created dynamically when this module is imported for the first time. In itself, it is empty and only to be used internally.";
		
		wooInitialize();
	};
#else
	#ifdef WOO_DEBUG
		BOOST_PYTHON_MODULE(BOOST_PP_CAT(BOOST_PP_CAT(_cxxInternal,WOO_CXX_FLAVOR),_debug))
	#else
		BOOST_PYTHON_MODULE(BOOST_PP_CAT(_cxxInternal,WOO_CXX_FLAVOR))
	#endif
	{
		LOG_DEBUG_EARLY("Initializing the _cxxInternal" BOOST_PP_STRINGIZE(WOO_CXX_FLAVOR) " module.");
		py::scope().attr("__doc__")="This module's binary contains all compiled Woo modules (such as :obj:`woo.core`), which are created dynamically when this module is imported for the first time. In itself, it is empty and only to be used internally.";
		// call automatically at module import time
		wooInitialize();
	}
#endif
