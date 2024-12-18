#pragma once
#include"../supp/object/Object.hpp"

struct Scene;

struct SceneAttachedObject: public Object{
	shared_ptr<Scene> getScene_py();
	#define woo_core_SceneAttachedObject__CLASS_BASE_DOC_ATRRS_PY \
		SceneAttachedObject,Object,"Parent class for object uniquely attached to one scene, for convenience of derived classes.", \
		((weak_ptr<Scene>,scene,,AttrTrait<Attr::readonly>().noGui().noDump(),"Back-reference to the scene object, needed for python; set automatically in Scene::postLoad when the object is assigned.")) \
		, /* py */ .add_property_readonly("scene",&Plot::getScene_py,"Back-reference to the scene object, needed for python; set automatically when the object is assigned to :obj:`Scene`.")

	WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_core_SceneAttachedObject__CLASS_BASE_DOC_ATRRS_PY);
};
WOO_REGISTER_OBJECT(SceneAttachedObject);


struct SceneCtrl: public SceneAttachedObject{
	#define woo_core_SceneCtrl__CLASS_BASE_DOC  \
		SceneCtrl,SceneAttachedObject,"Parent class for exposing higl-level controls for particular scene setups. Intended to be deruved from in python."
	WOO_DECL__CLASS_BASE_DOC(woo_core_SceneCtrl__CLASS_BASE_DOC);
};
WOO_REGISTER_OBJECT(SceneCtrl);

namespace woo{
	struct Plot: public SceneAttachedObject{
		void _resetPyObjects(); // called at regular shutdown (see py/system.py:setExitHandlers)
		#define woo_core_Plot__CLASS_BASE_DOC_ATTRS_PY \
			/* class, base, doc */ \
			Plot,SceneAttachedObject,"Storage for plots updated during simulation.", \
			/* attrs */ \
			/* since Scene.plot is noGui, we don't have to specify noGui() here for attributes (there are no handlers for dicts etc in the gui code) */ \
			((py::dict,data,,,"Global dictionary containing all data values, common for all plots, in the form {'name':[value,...],...}. Data should be added using plot.addData function. All [value,...] columns have the same length, they are padded with NaN if unspecified.")) \
			((py::dict,imgData,,,"Dictionary containing lists of strings, which have the meaning of images corresponding to respective :obj:`woo.plot.data` rows. See :obj:`woo.plot.plots` on how to plot images.")) \
			((py::dict,plots,,,"dictionary x-name -> (yspec,...), where yspec is either y-name or (y-name,'line-specification'). If ``(yspec,...)`` is ``None``, then the plot has meaning of image, which will be taken from respective field of :obj:`woo.plot.imgData`.")) \
			((py::dict,labels,,,"Dictionary converting names in data to human-readable names (TeX names, for instance); if a variable is not specified, it is left untranslated.")) \
			((py::dict,xylabels,,,"Dictionary of 2-tuples specifying (xlabel,ylabel) for respective plots; if either of them is None, the default auto-generated title is used.")) \
			((py::tuple,legendLoc,py::make_tuple("upper left","upper right"),,"Location of the y1 and y2 legends on the plot, if y2 is active.")) \
			((Real,axesWd,1,,"Linewidth (in points) to make *x* and *y* axes better visible; not activated if non-positive.")) \
			((py::list,currLineRefs,,AttrTrait<Attr::noSave>().noGui(),"References to axes which are being shown. Internal use only.")) \
			((string,annotateFmt," {xy[1]:.4g}",,"Format for annotations in plots; if empty, no annotation is shown; has no impact on existing plots. *xy* is 2-tuple of the current point in data space.")) \
			,/*py*/.def("_resetPyObjects",&Plot::_resetPyObjects)

		WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_core_Plot__CLASS_BASE_DOC_ATTRS_PY);
	};
};
// must be without woo:: namespace
WOO_REGISTER_OBJECT(Plot);
