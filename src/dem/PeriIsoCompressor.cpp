
// 2009 © Václav Šmilauer <eudoxos@arcig.cz>

#include"../core/Scene.hpp"
#include"PeriIsoCompressor.hpp"
#include"Particle.hpp"
#include"Funcs.hpp"
#include"FrictMat.hpp"
#include"Leapfrog.hpp"
#include"../supp/pyutil/gil.hpp"

WOO_IMPL_LOGGER(PeriIsoCompressor);

WOO_PLUGIN(dem,(PeriIsoCompressor))
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_PeriIsoCompressor__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_WeirdTriaxControl__CLASS_BASE_DOC_ATTRS);



void PeriIsoCompressor::run(){
	dem=static_cast<DemField*>(field.get());

	if(!scene->isPeriodic){ LOG_FATAL("Being used on non-periodic simulation!"); throw; }
	if(state>=stresses.size()) return;
	// initialize values
	if(charLen<=0){
		if(dem->particles->size()>0 && (*dem->particles)[0] && (*dem->particles)[0]->shape && (*dem->particles)[0]->shape->bound){
			const Bound& bv=*((*dem->particles)[0]->shape->bound);
			const Vector3r sz=bv.max-bv.min;
			charLen=(sz[0]+sz[1]+sz[2])/3.;
			LOG_INFO("No charLen defined, taking avg bbox size of body #0 = {}",charLen);
		} else { LOG_FATAL("No charLen defined and body #0 does not exist has no bound"); throw; }
	}
	Real maxDisplPerStep=2e-3*charLen;// this should be tunable somehow…
	const long& step=scene->step;
	Vector3r cellSize=scene->cell->getSize(); //unused: Real cellVolume=cellSize[0]*cellSize[1]*cellSize[2];
	Vector3r cellArea=Vector3r(cellSize[1]*cellSize[2],cellSize[0]*cellSize[2],cellSize[0]*cellSize[1]);
	const Real& maxSize=cellSize.maxCoeff();
	if(((step%globalUpdateInt)==0) || isnan(avgStiffness) || isnan(sigma[0]) || isnan(sigma[1])|| isnan(sigma[2])){
		Matrix3r TT=Matrix3r::Zero(); Matrix6r EE=Matrix6r::Zero();
		std::tie(TT,EE)=DemFuncs::stressStiffness(scene,dem,/*skipMultinodal*/false,/*volume*/-1.);
		sigma=TT.diagonal();
		avgStiffness=EE.topLeftCorner<3,3>().trace()/3.;
	}
	Real sigmaGoal=stresses[state]; assert(sigmaGoal<0);
	// expansion of cell in this step (absolute length)
	Vector3r cellGrow(Vector3r::Zero());
	// is the stress condition satisfied in all directions?
	bool allStressesOK=true;
	if(keepProportions){ // the same algo as below, but operating on quantitites averaged over all dimensions
		Real sigAvg=sigma.sum()/3., avgArea=cellArea.sum()/3., avgSize=cellSize.sum()/3.;
		Real avgGrow=(sigmaGoal-sigAvg)*avgArea/(avgStiffness>0?avgStiffness:1);
		Real maxToAvg=maxSize/avgSize;
		if(abs(maxToAvg*avgGrow)>maxDisplPerStep) avgGrow=Mathr::Sign(avgGrow)*maxDisplPerStep/maxToAvg;
		if(avgStiffness>0) { sigma+=(avgGrow*avgStiffness)*Vector3r::Ones(); sigAvg+=avgGrow*avgStiffness; }
		if(abs((sigAvg-sigmaGoal)/sigmaGoal)>5e-3) allStressesOK=false;
		cellGrow=(avgGrow/avgSize)*cellSize;
	}
	else{ // handle each dimension separately
		for(int axis=0; axis<3; axis++){
			// Δσ=ΔεE=(Δl/l)×(l×K/A) ↔ Δl=Δσ×A/K
			cellGrow[axis]=(sigmaGoal-sigma[axis])*cellArea[axis]/(avgStiffness>0?avgStiffness:1);  // FIXME: wrong dimensions? See PeriTriaxController
			if(abs(cellGrow[axis])>maxDisplPerStep) cellGrow[axis]=Mathr::Sign(cellGrow[axis])*maxDisplPerStep;
			// crude way of predicting sigma, for steps when it is not computed from intrs
			if(avgStiffness>0) sigma[axis]+=cellGrow[axis]*avgStiffness; // FIXME: dimensions
			if(abs((sigma[axis]-sigmaGoal)/sigmaGoal)>5e-3) allStressesOK=false;
		}
	}
	TRVAR4(cellGrow,sigma,sigmaGoal,avgStiffness);
	assert(scene->dt>0);
	for(int axis=0; axis<3; axis++){ scene->cell->nextGradV(axis,axis)=cellGrow[axis]/(scene->dt*scene->cell->getSize()[axis]); }

	// handle state transitions
	if(allStressesOK){
		if((step%globalUpdateInt)==0 || isnan(currUnbalanced)) currUnbalanced=DemFuncs::unbalancedForce(scene,dem,/*useMaxForce=*/false);
		if(currUnbalanced<maxUnbalanced){
			state+=1;
			// sigmaGoal reached and packing stable
			if(state==stresses.size()){ // no next stress to go for
				LOG_INFO("Finished");
				if(!doneHook.empty()){ LOG_DEBUG("Running doneHook: {}",doneHook); Engine::runPy("PeriIsoCompressor",doneHook); }
			} else { LOG_INFO("Loaded to {} done, going to {} now",sigmaGoal,stresses[state]); }
		} else {
			if((step%globalUpdateInt)==0){ LOG_DEBUG("Stress={}, goal={}, unbalanced={}",sigma,sigmaGoal,currUnbalanced); }
		}
	} else {
		// currUnbalanced=NaN; // if stresses not ok, don't report currUnbalanced as it was not computed.
	}
}



WOO_IMPL_LOGGER(WeirdTriaxControl);
WOO_PLUGIN(dem,(WeirdTriaxControl))

void WeirdTriaxControl::run(){
	dem=static_cast<DemField*>(field.get());
	if (!scene->isPeriodic){ throw runtime_error("WeirdTriaxControl run on aperiodic simulation."); }
	bool doUpdate((scene->step%globUpdate)==0);
	if(!leapfrogChecked){
		bool seenMe=false;
		for(const auto& e: scene->engines){
			if(e.get()==this) seenMe=true;
			if(e->isA<Leapfrog>() && !seenMe) LOG_ERROR("WeirdTriaxControl should always come **before** Leapfrog in the engine sequence, as it sets nextGradV. You can ignore this warning at your own risk.");
		}
		leapfrogChecked=true;
	}
	if(doUpdate){
		//"Natural" strain, still correct for large deformations, used for comparison with goals
		for (int i=0;i<3;i++) strain[i]=log(scene->cell->trsf(i,i));
		Matrix6r stiffness;
		std::tie(stress,stiffness)=DemFuncs::stressStiffness(scene,dem,/*skipMultinodal*/false,scene->cell->getVolume()*relVol);
	}
	if(isnan(mass) || mass<=0){ throw std::runtime_error("WeirdTriaxControl.mass must be positive, not "+to_string(mass)); }

	bool allOk=true;

	maxStrainedStress=NaN;
	// maximum stress along strain-prescribed axes
	for(int ax:{0,1,2}){ if((stressMask&(1<<ax))==0 && (maxStrainedStress<abs(stress(ax,ax)) || isnan(maxStrainedStress))) maxStrainedStress=abs(stress(ax,ax)); }


	// apply condition along each axis separately (stress or strain)
	for(int axis=0; axis<3; axis++){
 		Real& strain_rate=scene->cell->nextGradV(axis,axis);//strain rate on axis
		if(stressMask & (1<<axis)){   // control stress
			assert(mass>0);//user set
			Real dampFactor=1-growDamping*Mathr::Sign(strain_rate*(goal[axis]-stress(axis,axis)));
			strain_rate+=dampFactor*scene->dt*(goal[axis]-stress(axis,axis))/mass;
			LOG_TRACE("{}: stress={}, goal={}, gradV={}",axis,stress(axis,axis),goal[axis],strain_rate);
		} else {
			// control strain, see "true strain" definition here http://en.wikipedia.org/wiki/Finite_strain_theory
			strain_rate=(exp(goal[axis]-strain[axis])-1)/scene->dt;
			LOG_TRACE("{}: strain={}, goal={}, cellGrow={}",axis,strain[axis],goal[axis],strain_rate*scene->dt);
		}
		// limit maximum strain rate
		if (abs(strain_rate)>maxStrainRate[axis]) strain_rate=Mathr::Sign(strain_rate)*maxStrainRate[axis];

		// crude way of predicting stress, for steps when it is not computed from intrs
		if(doUpdate) LOG_DEBUG("{}: cellGrow={}, new stress={}, new strain={}",axis,strain_rate*scene->dt,stress(axis,axis),strain[axis]);
		// used only for temporary goal comparisons. The exact value is assigned in strainStressStiffUpdate
		strain[axis]+=strain_rate*scene->dt;
		// signal if condition not satisfied
		if(stressMask&(1<<axis)){
			Real curr=stress(axis,axis);
			if(goal[axis]!=0 && relStressTol>0. && abs((curr-goal[axis])/goal[axis])>relStressTol) allOk=false;
			else if(relStressTol<0 && !isnan(maxStrainedStress) && abs((curr-goal[axis])/maxStrainedStress)>abs(relStressTol)) allOk=false;
			else if(absStressTol>0 && abs(curr-goal[axis])>absStressTol) allOk=false;
		}else{
			Real curr=strain[axis];
			// since strain is prescribed exactly, tolerances need just to accomodate rounding issues
			if((goal[axis]!=0 && abs((curr-goal[axis])/goal[axis])>1e-6) || abs(curr-goal[axis])>1e-6){
				allOk=false;
				if(doUpdate) LOG_DEBUG("Strain not OK; {}>1e-6",abs(curr-goal[axis]));}
		}
	}
 	for (int k=0;k<3;k++) strainRate[k]=scene->cell->nextGradV(k,k);
	//Update energy input
	Real dW=(scene->cell->nextGradV*stress).trace()*scene->dt*scene->cell->getVolume();
	externalWork+=dW;
	if(scene->trackEnergy) scene->energy->add(-dW,"gradVWork",gradVWorkIx,/*non-incremental*/false);
	prevGrow=strainRate;

	if(allOk){
		if(doUpdate || currUnbalanced<0){
			currUnbalanced=DemFuncs::unbalancedForce(scene,dem,/*useMaxForce*/false);
			LOG_DEBUG("Stress/strain={},{},{}, goal={}, unbalanced={}",(stressMask&1?stress(0,0):strain[0]),(stressMask&2?stress(1,1):strain[1]),(stressMask&4?stress(2,2):strain[2]),goal,currUnbalanced);
		}
		if(currUnbalanced<maxUnbalanced){
			if (!doneHook.empty()){
				LOG_DEBUG("Running doneHook: {}",doneHook);
				Engine::runPy("WeirdTriaxControl",doneHook);
			}
		}
	}
}
