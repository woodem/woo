#pragma once

// macro to set the same docstring generation options in all modules
#ifdef WOO_NANOBIND
   #define WOO_SET_DOCSTRING_OPTS
#else
   #define WOO_SET_DOCSTRING_OPTS py::options options; options.disable_function_signatures();
#endif

