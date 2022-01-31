#pragma once
#include"../supp/object/Object.hpp"

struct Scene; 
struct Preprocessor: public Object{
	virtual shared_ptr<Scene> operator()();
	#define woo_core_Preprocessor_CLASS_BASE_DOC_ATTRS_PY \
		Preprocessor,Object,"Subclasses of this class generate a Scene object when called, based on their attributes." \
		, /* attrs */ \
		((string,title,"",,"Simulation title, will be assigned to the {title} tag, unless running in batch (where the batch title will be used.")) \
		, /* py */ .def("__call__",&Preprocessor::operator())
	WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_core_Preprocessor_CLASS_BASE_DOC_ATTRS_PY);
};
WOO_REGISTER_OBJECT(Preprocessor);
