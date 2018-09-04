#pragma once

#ifdef WOO_OPENGL
#include<woo/lib/object/Object.hpp>
#include<woo/lib/pyutil/except.hpp>
#include<boost/algorithm/string/predicate.hpp>
#include<typeindex>

/*
This class tries to be very user-friendly and is convolved. Explanation and rationale follow.

Objectives:
* store all OpenGL-related settings in one class, which does not have to be modified if new rendering classes are added (only if new dispatcher types are added)
* be user-friendly in terms of accessing all settings by their class name (or some variant thereof) as attribute of GlSetup in scripts
* Be able to move all settings away, apply new ones and so on
* Be able to have the class in woo.core, and make it compilable (the declarations at least) without including anything from woo.gl (therefore base everyting on woo.core.Object and cast and type-check at runtime)

Technique:
* store all classes in objs (read-only from python) and create accessors which have names derived from those objects (remove leading Gl1_, if any, and de-capitalize);
* accessors are created as lambda functions passed to boost::python, which makes them a bit hairy
* accessors have to check that correct type is assigned, also handling non-woo.core.Objects, None Objects
* accessors must give nice message when an incorrect type is passed
* correct types (as std::type_index), human-readable type-names and accessor names are remembered in objTypeIndices, objTypeNames, objAccessorNames filled at startup (when the class is registered)

Interface:
* GlSetup(..) can take both non-keyword argument (which are put where they should be, if they have correct type), and also keyword arguments, which are named the same as accessors
* If the inial *objs* is incorrect due to mismatched types, GlSetup reverts to defaults rather than refusing the input. This is useful for backwards-compatibility.
* individual functors can be assigned, e.g. S.gl.sphere=Gl1_Sphere(wire=True)
* parameters of functors can be changed, e.g. S.gl.sphere.wire=True

TODO:
* when an individual functor is assigned, mark objs dirty, so that Renderer can pick them up when run the next time (some synchronization might be necessary to avoid concurrent access), or sync those right away.
* Renderer is always the first in objs
* Scene must keep track of the Renderer to call, Renderer must keep track of functors and scene as well
* All currently used static data in Renderer (like fastDraw and selectedObject) must be moved somewhere inside Scene

*/

struct Renderer;

struct GlSetup: public Object{
	static string accessorName(const string& s);
	void checkIndex(int i) const;
	static vector<shared_ptr<Object>> makeObjs(); 
	// with constructed in c++, call this afterwars
	void init(){ objs=makeObjs(); }
	void ensureObjs();
	shared_ptr<Renderer> getRenderer();


	static vector<std::type_index> objTypeIndices;
	static vector<string> objTypeNames;
	static vector<string> objAccessorNames;
	py::list getObjNames() const;
	
	void postLoad(GlSetup&,void*);
	void pyHandleCustomCtorArgs(py::args_& args, py::kwargs& kw) override;
	// proxy for raw_function, extracts instance from args[0]
	// return value and kw only to satisfy interface
	// http://stackoverflow.com/questions/27488096/boost-python-raw-function-method
	static py::object pyCallStatic(py::tuple args, py::dict kw);
	void pyCall(const py::tuple& args);

	WOO_DECL_LOGGER;

	#define woo_gl_GlSetup__CLASS_BASE_DOC_ATTRS_PY \
		GlSetup,Object,"Proxy object for accessing all GL-related settings uniformly from the scene.", \
		((vector<shared_ptr<Object>>,objs,,AttrTrait<Attr::readonly>().noGui(),"List of all objects used; their order is determined at run-time. Some of them may be None (unused indices) which indicate separator in list of those objects when presented in the UI.")) \
		((bool,dirty,false,AttrTrait<Attr::readonly>().noGui().noDump(),"Set after modifying functors, so that they can be regenerated.")) \
		((string,qglviewerState,"",AttrTrait<Attr::readonly>().noGui(),"XML representation of the view state -- updated occasionally (once a second) from the current open view (if any).")) \
		,/*py*/ .add_property("objNames",&GlSetup::getObjNames).def("__call__",py::raw_function(&GlSetup::pyCallStatic,/*the instance*/1),"Replace all current functors by those passed as arguments."); \
			auto oo=makeObjs(); assert(objTypeIndices.empty()); \
			for(size_t i=0; i<oo.size(); i++){ \
				const auto& o=oo[i]; const auto oPtr=o.get(); \
				string accessor=accessorName(o?o->getClassName():""); \
				objTypeIndices.push_back(o?std::type_index(typeid(*oPtr)):std::type_index(typeid(void))); \
				objTypeNames.push_back(o?o->getClassName():"None"); \
				objAccessorNames.push_back(accessor); \
				/*separator*/if(!o) continue; \
				_classObj.add_property(accessor.c_str(),\
					/* see https://mail.python.org/pipermail/cplusplus-sig/2009-February/014263.html and http://stackoverflow.com/questions/16845547/using-c11-lambda-as-accessor-function-in-boostpythons-add-property-get-sig */ \
					py::detail::make_function_aux([i](shared_ptr<GlSetup> g){g->checkIndex(i); return g->objs[i];},py::default_call_policies(),boost::mpl::vector<shared_ptr<Object>,shared_ptr<GlSetup>>()), \
					py::detail::make_function_aux([i](shared_ptr<GlSetup> g, shared_ptr<Object> val){ g->checkIndex(i); if(!val) woo::ValueError("Must not be None."); const auto valPtr=val.get(); if(std::type_index(typeid(*valPtr))!=objTypeIndices[i]) woo::TypeError(objTypeNames[i]+" instance required (not "+val->getClassName()+")."); g->objs[i]=val; },py::default_call_policies(),boost::mpl::vector<void,shared_ptr<GlSetup>,shared_ptr<Object>>()) \
				); \
			}
	
	WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_gl_GlSetup__CLASS_BASE_DOC_ATTRS_PY);

};
WOO_REGISTER_OBJECT(GlSetup);

#endif /* WOO_OPENGL */
