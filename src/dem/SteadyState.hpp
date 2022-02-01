#pragma once

#include"Inlet.hpp"
#include"Outlet.hpp"

struct DetectSteadyState: public PeriodicEngine{
	WOO_DECL_LOGGER;
	enum{STAGE_INIT=0,STAGE_FLOW,STAGE_TRANS,STAGE_STEADY,STAGE_DONE};
	void run() override;
	#define woo_dem_DetectSteadyState__CLASS_BASE_DOC_ATTRS_PY \
		DetectSteadyState,PeriodicEngine,ClassTrait().doc("Detect steady state from summary flows of relevant inlets (total influx) and outlets (total efflux), plus waiting times in-between." \
		"The detection is done is several stages:\n\n" \
"1. (init) wait for :obj:`waitInit` before doing anything else;\n" \
"2. (flow) execute :obj:`hookFlow`, check if :math:`\\sum_i o_i \\geq \\alpha \\sum_j i_j` (:math:`\\alpha` is :obj:`relFlow`); if true, proto-steady state was reached and we proceed;\n" \
"3. (trans) execute :obj:`hookTrans`, then wait for :obj:`waitTrans`, then proceed to the next stage;\n" \
"4. (steady) executed :obj:`hookSteady`, then wait for :obj:`waitSteady`, then proceed;\n" \
"5. (done) execute :obj:`hookDone` and do nothing more.\n\n"), \
		((Real,waitInit,0,AttrTrait<>().timeUnit(),"Time to wait in ``init`` stage, before moving to ``flow``.")) \
		((Real,relFlow,1.,AttrTrait<>().timeUnit(),"Relative flow used for comparing influx and efflux in the ``flow`` stage.")) \
		((Real,waitTrans,0,AttrTrait<>().timeUnit(),"Time to wait in ``trans`` stage, before moving to ``steady``.")) \
		((Real,waitSteady,0,AttrTrait<>().timeUnit(),"Time to wait in the steady stage, before moving to ``done``.")) \
		((string,hookFlow,"",,"Hook executed when the ``flow`` stage is entered.")) \
		((string,hookTrans,"",,"Hook executed when the ``trans`` stage is entered.")) \
		((string,hookSteady,"",,"Hook executed when the ``steady`` stage is entered.")) \
		((string,hookDone,"e.dead=True",,"Hook executed when the ``done`` stage is entered.")) \
		((Real,rateSmooth,1,AttrTrait<>().range(Vector2r(0,1)),"Smoothing factor for rates ∈〈0,1〉")) \
		((int,stage,STAGE_INIT,AttrTrait<Attr::namedEnum>().readonly().namedEnum({{STAGE_INIT,{"init"}},{STAGE_FLOW,{"flow"}},{STAGE_TRANS,{"trans"}},{STAGE_STEADY,{"steady"}},{STAGE_DONE,{"done"}}}),"Stage in which we currently are.")) \
		((Real,stageEntered,0.,AttrTrait<>().readonly().timeUnit(),"Time when the current stage was entered.")) \
		((vector<shared_ptr<Inlet>>,inlets,,,"Inlets of which rates are used to compute summary influx.")) \
		((vector<shared_ptr<Outlet>>,outlets,,,"Inlets of which rates are used to compute summary efflux.")) \
		((Real,influx,NaN,AttrTrait<>().readonly(),"Smoothed summary influx value.")) \
		((Real,efflux,NaN,AttrTrait<>().readonly(),"Smoothed summary efflux value.")) \
		,/*py*/ .def_readonly("stageNum",&DetectSteadyState::stage,"Return the current :obj:`stage` as numerical value (useful for plotting).");

	WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_DetectSteadyState__CLASS_BASE_DOC_ATTRS_PY);
};
WOO_REGISTER_OBJECT(DetectSteadyState);


