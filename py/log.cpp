#include<string>
#include<woo/lib/base/Logging.hpp>
#include<woo/lib/base/Types.hpp>
#include<woo/lib/pyutil/doc_opts.hpp>
#include<woo/core/Master.hpp>

static std::shared_ptr<spdlog::logger> logLogger=spdlog::stdout_color_mt("woo.log");
int logGetLevel(std::string loggerName){
	auto log=spdlog::get(loggerName);
	if(!log) throw std::invalid_argument("No logger named '"+loggerName+"'.");
	return (int)log->level();
}
void logSetLevel(std::string loggerName, int level){
	const int mn=(int)spdlog::level::level_enum::trace; const int mx=(int)spdlog::level::level_enum::off;
	if(level<mn || level>mx) throw std::runtime_error("Invalid logger level value (not in range TRACE..OFF, i.e. "+to_string(mn)+".."+to_string(mx)+")");
	// std::string fullName(loggerName.empty()?"woo":("woo."+loggerName));
	std::shared_ptr<spdlog::logger> log;
	// apply to all loggers (TODO: match wildcard)
	if(loggerName.empty()){
		spdlog::apply_all([&](std::shared_ptr<spdlog::logger> l){
			l->set_level((spdlog::level::level_enum)level);
		});
	} else {
		log=spdlog::get(loggerName);
		if(!log){ SPDLOG_LOGGER_WARN(logLogger,"No logger named '{}', ignoring level setting.",loggerName); return; }
		log->set_level((spdlog::level::level_enum)level);
	}
}
// void logLoadConfig(std::string f){ log4cxx::PropertyConfigurator::configure(f); }
void logLoadConfig(std::string f){ throw std::runtime_error("logLoadConfig: not supported with spdlog."); }

WOO_PYTHON_MODULE(log);
PYBIND11_MODULE(log,mod){
	mod.doc() = "Access and manipulation of log4cxx loggers.";
	WOO_SET_DOCSTRING_OPTS;
	mod.def("level",logGetLevel,WOO_PY_ARGS(py::arg("logger")));
	mod.def("setLevel",logSetLevel,WOO_PY_ARGS(py::arg("logger"),py::arg("level")),"Set minimum severity *level* (constants ``TRACE``, ``DEBUG``, ``INFO``, ``WARN``, ``ERROR``, ``FATAL`` = ``CRITICAL`` and ``OFF``) for given logger. If the logger is '', the root logger 'woo' will be operated on.");
	mod.def("loadConfig",logLoadConfig,WOO_PY_ARGS(py::arg("fileName")),"Load configuration from file (log4cxx::PropertyConfigurator::configure)");
	mod.attr("TRACE")=   (int)spdlog::level::level_enum::trace;
	mod.attr("DEBUG")=   (int)spdlog::level::level_enum::debug;
	mod.attr("INFO")=    (int)spdlog::level::level_enum::info;
	mod.attr("WARN")=    (int)spdlog::level::level_enum::warn;
	mod.attr("ERROR")=   (int)spdlog::level::level_enum::err;
	mod.attr("FATAL")=   (int)spdlog::level::level_enum::critical;
	mod.attr("CRITICAL")=(int)spdlog::level::level_enum::critical;
	mod.attr("OFF")=     (int)spdlog::level::level_enum::off;
}
