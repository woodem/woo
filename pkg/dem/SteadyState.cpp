#include<woo/pkg/dem/SteadyState.hpp>

WOO_PLUGIN(dem,(DetectSteadyState));
WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_DetectSteadyState__CLASS_BASE_DOC_ATTRS_PY);
WOO_IMPL_LOGGER(DetectSteadyState);

void DetectSteadyState::run(){
	// compute smoothed flows in whatever the stage is
	Real sumInflux=0, sumEfflux=0;
	for(const auto& i: inlets)  sumInflux+=i->currRate;
	for(const auto& o: outlets) sumEfflux+=o->currRate;
	if(stepPrev<0 || isnan(influx) || isnan(rateSmooth)) influx=sumInflux;
	else influx=(1-rateSmooth)*influx+rateSmooth*sumInflux;
	if(stepPrev<0 || isnan(efflux) || isnan(rateSmooth)) efflux=sumEfflux;
	else efflux=(1-rateSmooth)*efflux+rateSmooth*sumEfflux;
	// if run for the very first time, set time
	if(stepPrev<0) stageEntered=scene->time; 
	switch(stage){
		case STAGE_INIT: {
			if(scene->time-stageEntered<=waitInit) return;
			LOG_INFO("Entering 'flow' stage at {}",scene->time);
			stage=STAGE_FLOW;
			stageEntered=scene->time;
			if(!hookFlow.empty()){
				LOG_DEBUG("Executing hookFlow: {}",hookFlow);
				runPy("DetectSteadyState.hookFlow",hookFlow);
			}
			break;
		}
		case STAGE_FLOW: {
			if(efflux<influx*relFlow) return;
			LOG_INFO("Entering 'trans' stage at {}",scene->time);
			stage=STAGE_TRANS;
			stageEntered=scene->time;
			if(!hookTrans.empty()){
				LOG_DEBUG("Executing hookTrans: {}",hookTrans);
				runPy("DetectSteadyState.hookTrans",hookTrans);
			}
			break;
		}
		case STAGE_TRANS: {
			if(scene->time-stageEntered<=waitTrans) return;
			LOG_INFO("Entering 'steady' stage at {}",scene->time);
			stage=STAGE_STEADY;
			stageEntered=scene->time;
			if(!hookSteady.empty()){
				LOG_DEBUG("Executing hookSteady: {}",hookSteady);
				runPy("DetectSteadyState.hookSteady",hookSteady);
			}
			break;
		}
		case STAGE_STEADY: {
			if(scene->time-stageEntered<=waitSteady) return;
			LOG_INFO("Entering 'done' stage at {}",scene->time);
			stage=STAGE_DONE;
			stageEntered=scene->time;
			if(!hookDone.empty()){
				LOG_DEBUG("Executing hookDone: {}",hookDone);
				runPy("DetectSteadyState.hookDone",hookDone);
			}
			break;
		}
		case STAGE_DONE: return;
		default: throw std::logic_error("DetectSteadyState.stage: invalid value "+to_string(stage)+" (please report bug).");
	}
}
