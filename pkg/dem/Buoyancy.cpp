#include<woo/pkg/dem/Buoyancy.hpp>
WOO_PLUGIN(dem,(HalfspaceBuoyancy));
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_HalfspaceBuoyancy_CLASS_BASE_DOC_ATTRS);

void HalfspaceBuoyancy::postLoad(HalfspaceBuoyancy&, void*){
	// check for valid attributes
	if(!node) throw std::runtime_error("HalfspaceBuoyancy.node may not be None!");
}


void HalfspaceBuoyancy::run(){
	DemField& dem=field->cast<DemField>();
	for(const auto& p: *dem.particles){
		// check mask
		if(mask!=0 && ((mask&p->mask)==0)) continue;
		// check that the particle is uninodal
		if(!p->shape || p->shape->nodes.size()!=1) continue;
		// all uninodal particles should have volumetrically equivalent radius
		// we will treat everything as sphere (even Ellipsoid or Capsule)
		Real rad=p->shape->equivRadius();
		if(isnan(rad)) continue; // but we check anyway...
		// depth, in local coordinates
		Real h=node->glob2loc(p->shape->nodes[0]->pos).z();
		Real relh=h-waterHeight;
		Real relH=rad-relh; 
		Real vS = (4*M_PI*(rad*rad*rad))/3;
		Real vD = 0;
		Real grav = (node->ori.conjugate()*dem.gravity).z();
		// entirely above the liquid level, no force to apply
		if(h>=2*rad) continue;

		// force and torque applied to the particle (must be eventually applied in global coords!)
		// so make it clear what CS we use
		Vector3r F,T;

		// implement something meaningful here
		// gravity is dem.gravity (a Vector3r in global CS)
		F=T=Vector3r::Zero();

		// vector for drag force
		Vector3r Fd=Vector3r::Zero();
		// vector for drag torque
		Vector3r Md=Vector3r::Zero();
		// vector for buoyant force
		Vector3r Fb = Vector3r::Zero();

		// DemData instance for the node
		auto& dyn(p->shape->nodes[0]->getData<DemData>());
		const auto& velo = dyn.vel;
		const auto& angVelo = dyn.angVel;

		//Simple Buoyancy = Submerged Volume * Density of Water * Gravity
		//Drag forces are referenced from here http://www.tandfonline.com/doi/abs/10.1080/02726351.2010.544377
		if(relH<2*rad){
			if(waterHeight>h){
				vD=(M_PI*relH*relH*((3*rad)-relH))/3;
				Fb=Vector3r(0,0,liqRho*vD*-grav);	
				if(drag){
					Real alpha1=acos((waterHeight-h)/rad);
					Real area=2*M_PI*rad*rad*(M_PI-alpha1)/M_PI+rad*sin(alpha1)*(waterHeight-h);

					Fd =-(3/4)*liqRho*(dragCoef/2*rad)*velo*velo.norm();
					Md=-dragCoef*(liqRho/2)*angVelo*angVelo.norm()*pow(rad,5);
					
				}
			}
			else if(waterHeight<=h){
				vD=(M_PI*relH*relH*((3*rad)-relH))/3;
				Fb=Vector3r(0,0,liqRho*vD*-grav);
				if(drag){
					Real alpha1=acos((h-waterHeight)/rad);
					Real area=2*M_PI*rad*rad*alpha1/M_PI-rad*sin(alpha1)*(h-waterHeight);

					Fd =-(3/4)*liqRho*(dragCoef/2*rad)*velo*velo.norm();
					Md=-dragCoef*(liqRho/2)*angVelo*angVelo.norm()*pow(rad,5);
					
				}
			}
		}

		else{
			Fb=Vector3r(liqRho*vS*-grav);
			if(drag){
				Real area=M_PI*rad*rad;

				Fd =-(3/4)*liqRho*(dragCoef/2*rad)*velo*velo.norm();
				Md=-dragCoef*(liqRho/2)*angVelo*angVelo.norm()*pow(rad,5);
				
			}	
		}

		F += Fb;
		if(drag){
			F += Fd;
			T += Md;
		}
		// if we were in parallel section, use this for access sync: dyn.addForceTorque(F,T);
		dyn.force+=node->ori*F; dyn.torque+=node->ori*T;
	}
}

