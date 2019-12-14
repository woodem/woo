* boost::python::extract:

	→ py::isinstance(py::object), py::cast<T>(py::object)

	* compatibility wrapper in lib/pyutil/compat.hpp

* boost::python::object(a) to wrap any object as python object:

	→ py::cast(a);

* `no_init`: just not specifying init<>()??

* pickle: ?

* inherit from boost::noncopyable?

* `add_property` → `def_property` (getter and setter), `def_property_readonly` (getter only)

* `def_readonly`, `def_readwrite`: unchanged

* how to inherit from `boost::noncopyable` in `py::class_`??

* py::dict:
  *  	bp: `has_key`, pb11: `contains`
  *   bp::api::del(dict,key); pb11: dict.attr("pop")(key)

* !! no single way to get py::object from anything:
  * py::cast(...) works only for non-python types
  * py::object(...) works only for python types



Return value policies
-------------------------

`return_by_value` → `py::return_value_policy::copy`
