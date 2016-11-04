#include<woo/core/Master.hpp>
#include<woo/core/Plot.hpp>
#include<woo/core/Scene.hpp>
#include<woo/lib/pyutil/gil.hpp>


WOO_PLUGIN(core,(SceneAttachedObject)(SceneCtrl)(Plot));
WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_core_SceneAttachedObject__CLASS_BASE_DOC_ATRRS_PY);
shared_ptr<Scene> SceneAttachedObject::getScene_py(){ return scene.lock(); }
WOO_IMPL__CLASS_BASE_DOC(woo_core_SceneCtrl__CLASS_BASE_DOC);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_core_Plot__CLASS_BASE_DOC_ATTRS_PY);

void Plot::_resetPyObjects(){
	unique_ptr<GilLock> gil;
	if(Py_IsInitialized()){ gil=unique_ptr<GilLock>(new GilLock); }
	data=imgData=plots=labels=xylabels=py::dict();
	legendLoc=py::tuple();
	currLineRefs=py::object();
}


