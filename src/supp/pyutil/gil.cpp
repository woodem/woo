#include"gil.hpp"
void pyRunString(const std::string& cmd){
	py::gil_scoped_acquire lock; PyRun_SimpleString(cmd.c_str());
};

