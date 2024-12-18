#include"CrossAnisotropy.hpp"
#include"FrictMat.hpp"
#include"L6Geom.hpp"
#include"Sphere.hpp"

WOO_PLUGIN(dem,(Cp2_FrictMat_FrictPhys_CrossAnisotropic));
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_Cp2_FrictMat_FrictPhys_CrossAnisotropic__CLASS_BASE_DOC_ATTRS);

WOO_IMPL_LOGGER(Cp2_FrictMat_FrictPhys_CrossAnisotropic);

void Cp2_FrictMat_FrictPhys_CrossAnisotropic::postLoad(Cp2_FrictMat_FrictPhys_CrossAnisotropic&,void*){
	xisoAxis=Vector3r(cos(alpha)*sin(beta),-sin(alpha)*sin(beta),cos(beta));
	//OLD: Vector3r anisoNormal=Vector3r(cos(a)*sin(b),-sin(a)*sin(b),cos(b));
	//OLD: rot.setFromTwoVectors(Vector3r::UnitX(),anisoNormal);
	recomputeStep=scene->step+1; // recompute everything at the next step
	nu1=E1/(2*G1)-1;
}

void Cp2_FrictMat_FrictPhys_CrossAnisotropic::go(const shared_ptr<Material>& b1, const shared_ptr<Material>& b2, const shared_ptr<Contact>& C){
	if(C->phys && recomputeStep!=scene->step) return;
	// if(C->phys) cerr<<"Cp2_..._CrossAnisotropic: Recreating ##"<<C->leakPA()->id<<"+"<<C->leakPB()->id<<endl;
	C->phys=shared_ptr<CPhys>(new FrictPhys); // this deletes the old CPhys, if it was there
	FrictPhys& ph=C->phys->cast<FrictPhys>();
	L6Geom& g=C->geom->cast<L6Geom>();

	#if 0
		// this is fucked up, since we rely on data passed from the Cg2 functors
		Real A=M_PI*pow2(g.getMinRefLen()); // contact "area"
		Real l=g.lens[0]+g.lens[1]; // contact length
	#else
		const Particle *pA=C->leakPA(), *pB=C->leakPB();
		if(!dynamic_cast<Sphere*>(pA->shape.get()) || !dynamic_cast<Sphere*>(pB->shape.get())){
			LOG_FATAL("Cp2_FrictMat_FrictPhys_CrossAnisotropic: can be only used on spherical particles!");
			throw std::runtime_error("Cp2_FrictMat_FrictPhys_CrossAnisotropic: can be only used on spherical particles!");
		}
		Real r1=pA->shape->cast<Sphere>().radius, r2=pB->shape->cast<Sphere>().radius;
		Real A=M_PI*pow2(min(r1,r2));
		Real l=C->dPos(scene).norm(); // handles periodicity gracefully
	#endif

	// angle between pole (i.e. anisotropy normal) and contact normal
	Real sinTheta=(/*xiso axis in global coords*/xisoAxis.cross(/*contact axis in global coords*/g.node->ori*Vector3r::UnitX())).norm();

	//OLD: Real sinTheta=/*aniso z-axis in global coords*/((rot.conjugate()*Vector3r::UnitX()).cross(/*normal in global coords*/g.node->ori.conjugate()*Vector3r::UnitX())).norm();

	// cerr<<"x-aniso normal "<<Vector3r(rot.conjugate()*Vector3r::UnitZ())<<", contact axis "<<Vector3r(g.node->ori.conjugate()*Vector3r::UnitX())<<", angle "<<asin(sinTheta)*180/M_PI<<" (sin="<<sinTheta<<")"<<endl;
	Real weight=pow2(sinTheta);
	ph.kn=(A/l)*(weight*E1+(1-weight)*E2);
	ph.kt=(A/l)*(weight*G1+(1-weight)*G2);
	ph.tanPhi=min(b1->cast<FrictMat>().tanPhi,b2->cast<FrictMat>().tanPhi);
}

#if 0
#if WOO_OPENGL
WOO_PLUGIN(gl,(GlExtra_LocalAxes));
#endif	
#ifdef WOO_OPENGL
#include"../supp/opengl/GLUtils.hpp"

void GlExtra_LocalAxes::render(const GlViewInfo& glInfo){
	for(int ax=0; ax<3; ax++){
		Vector3r c(Vector3r::Zero()); c[ax]=1; // color and, at the same time, local axis unit vector
		//GLUtils::GLDrawArrow(pos,pos+length*(ori*c),c);
	}
	#if 0
		Real realSize=2*length;
		int nSegments=20;
		if(grid & 1) { glColor3f(0.6,0.3,0.3); glPushMatrix(); glRotated(90.,0.,1.,0.); QGLViewer::drawGrid(realSize,nSegments); glPopMatrix();}
		if(grid & 2) { glColor3f(0.3,0.6,0.3); glPushMatrix(); glRotated(90.,1.,0.,0.); QGLViewer::drawGrid(realSize,nSegments); glPopMatrix();}
		if(grid & 4) { glColor3f(0.3,0.3,0.6); glPushMatrix(); /*glRotated(90.,0.,1.,0.);*/ QGLViewer::drawGrid(realSize,nSegments); glPopMatrix();}
	#endif
};

#endif
#endif
