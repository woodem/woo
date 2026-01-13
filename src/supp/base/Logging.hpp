// 2006-2008 © Václav Šmilauer <eudoxos@arcig.cz> 
#pragma once
#include"Math.hpp"
/*
 * This file defines various useful logging-related macros - userspace stuff is
 * - LOG_* for actual logging,
 * - WOO_DECL_LOGGER; that should be used in class header to create separate logger for that class,
 * - WOO_IMPL_LOGGER(className); that must be used in class implementation file to create the static variable.
 *
 * Note that the latter 2 may change their name to something like LOG_DECLARE and LOG_CREATE, to be consistent.
 * Some other macros will be very likely added, to allow for easy variable tracing etc. Suggestions welcome.
 *
 * All of user macros should come in 2 flavors, depending on whether we use log4cxx or not (backward compatibility).
 * The default is not to use it, unless the preprocessor macro WOO_LOG4CXX is defined. In that case, you want to #include
 * woo-core/logging.h and link with log4cxx.
 *
 * TODO:
 * 1. [done] for optimized builds, at least debugging macros should become no-ops
 * 2. add the TRACE level; this has to be asked about on log4cxx mailing list perhaps. Other levels may follow easily. [will be done with log4cxx 0.10, once debian ships packages and we can safely migrate]
 *
 * For more information, see http://logging.apache.org/log4cxx/, especially the part on configuration files, that allow
 * very flexibe, runtime and fine-grained output redirections, filtering etc.
 *
 * Yade has the logging config file by default in ~/.woo-$VERSION/logging.conf.
 *
 */

#define _LOG_HEAD __FILE__ ":"<<__LINE__<<" "<<__FUNCTION__<<": "

// those work even in non-debug builds (startup diagnostics)
#define LOG_DEBUG_EARLY(msg) { if(getenv("WOO_DEBUG")) std::cerr<<"DEBUG "<<_LOG_HEAD<<msg<<std::endl; }
#define LOG_DEBUG_EARLY_FRAGMENT(msg) { if(getenv("WOO_DEBUG")) std::cerr<<msg; }

#include "Math.hpp"

#ifdef WOO_DEBUG
	#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#else
	#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#endif
#include<spdlog/spdlog.h>
#include<spdlog/async_logger.h>
#include<spdlog/sinks/stdout_color_sinks.h>
#include<spdlog/fmt/ostr.h>

#define LOG_TRACE(...) {SPDLOG_LOGGER_TRACE(logger,__VA_ARGS__);}
#define LOG_DEBUG(...) {SPDLOG_LOGGER_DEBUG(logger,__VA_ARGS__);}
#define LOG_INFO(...)  {SPDLOG_LOGGER_INFO(logger,__VA_ARGS__);}
#define LOG_WARN(...)  {SPDLOG_LOGGER_WARN(logger,__VA_ARGS__);}
#define LOG_ERROR(...) {SPDLOG_LOGGER_ERROR(logger,__VA_ARGS__);}
#define LOG_FATAL(...) {SPDLOG_LOGGER_CRITICAL(logger,__VA_ARGS__);}

#define WOO_DECL_LOGGER public: static std::shared_ptr<spdlog::logger> logger;
#define WOO_IMPL_LOGGER(classname) std::shared_ptr<spdlog::logger> classname::logger=spdlog::stdout_color_mt(#classname)
#define WOO_LOCAL_LOGGER(name) static std::shared_ptr<spdlog::logger> logger=spdlog::stdout_color_mt(#name)

// these macros are temporary
#define TRACE LOG_TRACE("Been here")
#define TRVAR1(a) LOG_TRACE("{}={}",#a,a);
#define TRVAR2(a,b) LOG_TRACE("{}={}, {}={}",#a,a,#b,b);
#define TRVAR3(a,b,c) LOG_TRACE("{}={}, {}={}, {}={}",#a,a,#b,b,#c,c);
#define TRVAR4(a,b,c,d) LOG_TRACE("{}={}, {}={}, {}={}, {}={}",#a,a,#b,b,#c,c,#d,d);
#define TRVAR5(a,b,c,d,e) LOG_TRACE("{}={}, {}={}, {}={}, {}={}, {}={}",#a,a,#b,b,#c,c,#d,d,#e,e);
#define TRVAR6(a,b,c,d,e,f) LOG_TRACE("{}={}, {}={}, {}={}, {}={}, {}={}, {}={}",#a,a,#b,b,#c,c,#d,d,#e,e,#f,f);

