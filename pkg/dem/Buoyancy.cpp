#include<woo/pkg/dem/Buoyancy.hpp>
WOO_PLUGIN(dem,(HalfspaceBuoyancy));
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_HalfspaceBuoyancy_CLASS_BASE_DOC_ATTRS);

void HalfspaceBuoyancy::run(){
	DemField& dem=field->cast<DemField>();
	Real gravNorm=dem.gravity.norm();
	if(gravNorm==0.) throw std::runtime_error("HalfspaceBuoyancy: DemField.gravity must not be zero.");
	// downwards unit vector, in the direction of gravity (≡ inner normal of the liquid surface)
	Vector3r gravDir=dem.gravity/gravNorm;
	for(const auto& p: *dem.particles){
		// check mask
		if(mask!=0 && ((mask&p->mask)==0)) continue;
		// check that the particle is uninodal
		if(!p->shape || p->shape->nodes.size()!=1) continue;
		// all uninodal particles should have volumetrically equivalent radius
		// we will treat everything as sphere (even Ellipsoid or Capsule)
		Real rad=p->shape->equivRadius();
		if(isnan(rad)) continue; // but we check anyway...
		// centroid above liquid surf (defined by surfPt and gravDir); if h<0 ↔ centroid below liquid surface
    	Real h=-(p->shape->nodes[0]->pos-surfPt).dot(gravDir);
		// particle entirely above the liquid level, no force to apply (check as soon as possible)
		if(h>=rad) continue;

		// depth of the bottom of the particle, below liquid surface (positive = submersion)
		Real dp=rad-h; 

		// DemData instance for the node
		auto& dyn(p->shape->nodes[0]->getData<DemData>());
		const auto& vel=dyn.vel; const auto& angVel=dyn.angVel;
		// will be modified in-place
		auto& F=dyn.force; auto& T=dyn.torque;

		// * simple buoyancy = submerged volume * density of water * gravity
		// * drag forces are referenced from http://www.tandfonline.com/doi/abs/10.1080/02726351.2010.544377
		
		if(dp<2*rad){
			if(dp<rad){ // less than half submerged
				Real vD=(M_PI*pow(dp,2)*((3*rad)-dp))/3;
				F+=-liqRho*vD*dem.gravity;	
				if(drag){
					// XXX: unused??
					// Real alpha1=acos((waterHeight-h)/rad);
					// Real area=2*M_PI*pow(rad,2)*(M_PI-alpha1)/M_PI+rad*sin(alpha1)*(waterHeight-h);
					// XXX: these should depend somehow on submerged volume or area??
					F+=-(3/4.)*liqRho*(dragCoef/2*rad)*vel*vel.norm(); // XXX: should this be dragCoef/(2*rad)?? [also note (3/4)==0 in c(++) whereas (3/4.)=(3./4.)=(3./4)=0.75, I fixed that]
					T+=-dragCoef*(liqRho/2)*angVel*angVel.norm()*pow(rad,5);
				}
			} else { // more than half submerged
				Real vD=(M_PI*pow(dp,2)*((3*rad)-dp))/3;
				F+=-liqRho*vD*dem.gravity;
				if(drag){
					// XXX: unused??
					// Real alpha1=acos((h-waterHeight)/rad);
					// Real area=2*M_PI*pow(rad,2)*alpha1/M_PI-rad*sin(alpha1)*(h-waterHeight);
					// XXX: these should depend somehow on submerged volume or area??
					F+=-(3/4.)*liqRho*(dragCoef/2*rad)*vel*vel.norm(); // XXX: should this be dragCoef/(2*rad)?? [also note (3/4)==0 in c(++) whereas (3/4.)=(3./4.)=(3./4)=0.75, I fixed that]
					T+=-dragCoef*(liqRho/2.)*angVel*angVel.norm()*pow(rad,5);
				}
			}
		} else { // fully submerged
			F+=-liqRho*p->shape->volume()*dem.gravity;
			if(drag){
				// Real area=M_PI*pow(rad,2); // XXX: unused?
				F+=-(3/4.)*liqRho*(dragCoef/2*rad)*vel*vel.norm();
				T+=-dragCoef*(liqRho/2)*angVel*angVel.norm()*pow(rad,5);
			}	
		}
	}
}

