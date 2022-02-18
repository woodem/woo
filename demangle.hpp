// https://stackoverflow.com/a/32884861
#include <cxxabi.h>
#include <memory>
#include <string>
#include <cassert>

template <typename T>
std::string _cxx_demangle()
{
    int status;
    std::unique_ptr<char> realname;
    const std::type_info  &ti = typeid(T);
    realname.reset(abi::__cxa_demangle(ti.name(), 0, 0, &status));
    assert(status == 0);
    return std::string(realname.get());
}
