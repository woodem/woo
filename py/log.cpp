#include<string>
#include<woo/lib/base/Logging.hpp>
#include<woo/lib/base/Types.hpp>
#include<woo/lib/pyutil/doc_opts.hpp>
#include<woo/core/Master.hpp>
enum{ll_TRACE,ll_DEBUG,ll_INFO,ll_WARN,ll_ERROR,ll_FATAL};

static std::shared_ptr<spdlog::logger> logLogger=spdlog::stdout_color_mt("woo.log");
void logSetLevel(std::string loggerName, int level){
	// std::string fullName(loggerName.empty()?"woo":("woo."+loggerName));
	spdlog::level::level_enum newLevel;
	switch(level){
			case ll_TRACE: newLevel=spdlog::level::level_enum::trace; break;
			case ll_DEBUG: newLevel=spdlog::level::level_enum::debug; break;
			case ll_INFO:  newLevel=spdlog::level::level_enum::info; break;
			case ll_WARN:  newLevel=spdlog::level::level_enum::warn; break;
			case ll_ERROR: newLevel=spdlog::level::level_enum::err; break;
			case ll_FATAL: newLevel=spdlog::level::level_enum::critical; break;
		default: throw std::invalid_argument("Unrecognized logging level "+to_string(level));
	}
	std::shared_ptr<spdlog::logger> log;
	// apply to all loggers (TODO: match wildcard)
	if(loggerName.empty()){
		spdlog::apply_all([&](std::shared_ptr<spdlog::logger> l){
			l->set_level(newLevel);
		});
	} else {
		log=spdlog::get(loggerName);
		if(!log){ SPDLOG_LOGGER_WARN(logLogger,"No logger named '{}', ignoring level setting.",loggerName); return; }
		log->set_level(newLevel);
	}
}
// void logLoadConfig(std::string f){ log4cxx::PropertyConfigurator::configure(f); }
void logLoadConfig(std::string f){ throw std::runtime_error("logLoadConfig: not supported with spdlog."); }

WOO_PYTHON_MODULE(log);
PYBIND11_MODULE(log,mod){
	mod.doc() = "Access and manipulation of log4cxx loggers.";
	WOO_SET_DOCSTRING_OPTS;

	mod.def("setLevel",logSetLevel,WOO_PY_ARGS(py::arg("logger"),py::arg("level")),"Set minimum severity *level* (constants ``TRACE``, ``DEBUG``, ``INFO``, ``WARN``, ``ERROR``, ``FATAL``) for given logger. \nLeading 'woo.' will be appended automatically to the logger name; if logger is '', the root logger 'woo' will be operated on.");
	mod.def("loadConfig",logLoadConfig,WOO_PY_ARGS(py::arg("fileName")),"Load configuration from file (log4cxx::PropertyConfigurator::configure)");
	mod.attr("TRACE")=(int)ll_TRACE;
	mod.attr("DEBUG")=(int)ll_DEBUG;
	mod.attr("INFO")= (int)ll_INFO;
	mod.attr("WARN")= (int)ll_WARN;
	mod.attr("ERROR")=(int)ll_ERROR;
	mod.attr("FATAL")=(int)ll_FATAL;
}
