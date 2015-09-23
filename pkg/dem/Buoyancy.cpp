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
		// entirely above the liquid level, no force to apply
		if(h>=2*rad) continue;

		// force and torque applied to the particle (must be eventually applied in global coords!)
		// so make it clear what CS we use
		Vector3r F,T;

		// implement something meaningful here
		// gravity is dem.gravity (a Vector3r in global CS)
		F=T=Vector3r::Zero();
		//F=Vector3r(0,0,-1*(node->ori.conjugate()*dem.gravity).z());
		//Buoyancy = Submerged Volume * Density of Water * Gravity
		Real vS = 4*M_PI*(rad*rad*rad);
		Real dens = 1000;
		F=Vector3r(0,0,-1*(vS*dens)*(node->ori.conjugate()*dem.gravity).z());


		// DemData instance for the node
		auto& dyn(p->shape->nodes[0]->getData<DemData>());
		// if we were in parallel section, use this for access sync: dyn.addForceTorque(F,T);
		dyn.force+=F; dyn.torque+=T;
	}
}

