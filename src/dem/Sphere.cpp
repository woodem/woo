#include"Sphere.hpp"
#include"ParticleContainer.hpp"

WOO_PLUGIN(dem,(Sphere)(Cg2_Sphere_Sphere_L6Geom)(Bo1_Sphere_Aabb)(In2_Sphere_ElastMat));

WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_Sphere__CLASS_BASE_DOC_ATTRS_CTOR);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_In2_Sphere_ElastMat__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_Bo1_Sphere_Aabb__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_Cg2_Sphere_Sphere_L6Geom__CLASS_BASE_DOC_ATTRS);



void woo::Sphere::selfTest(const shared_ptr<Particle>& p){
	if(radius<=0) throw std::runtime_error("Sphere #"+to_string(p->id)+": radius must be positive (not "+to_string(radius)+")");
	if(!numNodesOk()) throw std::runtime_error("Sphere #"+to_string(p->id)+": numNodesOk() failed (has "+to_string(nodes.size())+" nodes)");
	Shape::selfTest(p);
}

Real woo::Sphere::volume() const { return (4/3.)*M_PI*pow3(radius); }
void woo::Sphere::applyScale(Real scale) { radius*=scale; }

AlignedBox3r woo::Sphere::alignedBox() const {
	AlignedBox3r ret; ret.extend(nodes[0]->pos-radius*Vector3r::Ones()); ret.extend(nodes[0]->pos+radius*Vector3r::Ones()); return ret;
}

void woo::Sphere::lumpMassInertia(const shared_ptr<Node>& n, Real density, Real& mass, Matrix3r& I, bool& rotateOk){
	if(n.get()!=nodes[0].get()) return;
	checkNodesHaveDemData();
	rotateOk=true;
	Real m=(4/3.)*M_PI*pow3(radius)*density;
	mass+=m;
	I.diagonal()+=Vector3r::Ones()*(2./5.)*m*pow2(radius);
};

void woo::Sphere::setFromRaw(const Vector3r& _center, const Real& _radius, vector<shared_ptr<Node>>& nn, const vector<Real>& raw) {
	Shape::setFromRaw_helper_checkRaw_makeNodes(raw,0);
	radius=_radius;
	nodes[0]->pos=_center;
	nn.push_back(nodes[0]);
}
void woo::Sphere::asRaw(Vector3r& _center, Real& _radius, vector<shared_ptr<Node>>&nn, vector<Real>& raw) const {
	raw.resize(0);
	_center=nodes[0]->pos;
	_radius=radius;
}

bool woo::Sphere::isInside(const Vector3r& pt) const {
	assert(numNodesOk());
	return (pt-nodes[0]->pos).squaredNorm()<=pow2(radius);
}

void Bo1_Sphere_Aabb::go(const shared_ptr<Shape>& sh){
	Sphere& s=sh->cast<Sphere>();
	const Real& distFactor(field->cast<DemField>().distFactor);
	Vector3r halfSize=(distFactor>0?distFactor:1.)*s.radius*Vector3r::Ones();
	goGeneric(sh, halfSize);
}

void Bo1_Sphere_Aabb::goGeneric(const shared_ptr<Shape>& sh, Vector3r halfSize){
	if(!sh->bound){ sh->bound=make_shared<Aabb>(); /* ignore node rotation*/ sh->bound->cast<Aabb>().maxRot=-1; }
	Aabb& aabb=sh->bound->cast<Aabb>();
	assert(sh->numNodesOk());
	const Vector3r& pos=sh->nodes[0]->pos;
	if(!scene->isPeriodic){ aabb.min=pos-halfSize; aabb.max=pos+halfSize; return; }

	// adjust box size along axes so that sphere doesn't stick out of the box even if sheared (i.e. parallelepiped)
	halfSize=scene->cell->shearAlignedExtents(halfSize);
	#if 0
		if(scene->cell->hasShear()){
			Vector3r refHalfSize(halfSize);
			const Vector3r& cos=scene->cell->getCos();
			for(int i=0; i<3; i++){
				int i1=(i+1)%3,i2=(i+2)%3;
				halfSize[i1]+=.5*refHalfSize[i1]*(1/cos[i]-1);
				halfSize[i2]+=.5*refHalfSize[i2]*(1/cos[i]-1);
			}
		}
		//cerr<<" || "<<halfSize<<endl;
	#endif
	aabb.min=scene->cell->unshearPt(pos)-halfSize;
	aabb.max=scene->cell->unshearPt(pos)+halfSize;	
}



void Cg2_Sphere_Sphere_L6Geom::setMinDist00Sq(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const shared_ptr<Contact>& C){
	const Real& distFactor(field->cast<DemField>().distFactor);
	C->minDist00Sq=pow2(abs(distFactor*(s1->cast<Sphere>().radius+s2->cast<Sphere>().radius)));
}


bool Cg2_Sphere_Sphere_L6Geom::go(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const Vector3r& shift2, const bool& force, const shared_ptr<Contact>& C){
	const Real& r1=s1->cast<Sphere>().radius;
	const Real& r2=s2->cast<Sphere>().radius;
	assert(s1->numNodesOk()); assert(s2->numNodesOk());
	assert(s1->nodes[0]->hasData<DemData>()); assert(s2->nodes[0]->hasData<DemData>());
	const DemData& dyn1(s1->nodes[0]->getData<DemData>());
	const DemData& dyn2(s2->nodes[0]->getData<DemData>());
	const Real& distFactor(field->cast<DemField>().distFactor);


	Vector3r relPos=s2->nodes[0]->pos+shift2-s1->nodes[0]->pos;
	Real unDistSq=relPos.squaredNorm()-pow2(abs(distFactor)*(r1+r2));
	if (unDistSq>0 && !C->isReal() && !force) return false;

	// contact exists, go ahead

	Real dist=relPos.norm();
	Real uN=dist-(r1+r2);
	Vector3r normal=relPos/dist;
	Vector3r contPt=s1->nodes[0]->pos+(r1+0.5*uN)*normal;

	handleSpheresLikeContact(C,s1->nodes[0]->pos,dyn1.vel,dyn1.angVel,s2->nodes[0]->pos+shift2,dyn2.vel,dyn2.angVel,normal,contPt,uN,r1,r2);

	return true;

};


void In2_Sphere_ElastMat::go(const shared_ptr<Shape>& sh, const shared_ptr<Material>& m, const shared_ptr<Particle>& particle){
	if(!alreadyWarned_ContactLoopWithApplyForces){
		for(const auto& e: scene->engines){
			if(!e->isA<ContactLoop>()) continue;
			if(e->cast<ContactLoop>().applyForces){
				LOG_WARN("In2_Sphere_ElastMat: ContactLoop.applyForces==True, forces on spheres will be probably applied twice; are you sure this is what you want?!");
			}
		}
		alreadyWarned_ContactLoopWithApplyForces=true;
	}
	for(const Particle::MapParticleContact::value_type& I: particle->contacts){
		const shared_ptr<Contact>& C(I.second); if(!C->isReal()) continue;
		Vector3r F,T,xc;
		std::tie(F,T,xc)=C->getForceTorqueBranch(particle,/*nodeI*/0,scene);
		#ifdef WOO_DEBUG
			const Particle *pA=C->leakPA(), *pB=C->leakPB();
			bool isPA=(pA==particle.get()); // int sign=(isPA?1:-1);
			if(isnan(F[0])||isnan(F[1])||isnan(F[2])||isnan(T[0])||isnan(T[1])||isnan(T[2])){
				std::ostringstream oss; oss<<"NaN force/torque on particle #"<<particle->id<<" from ##"<<pA->id<<"+"<<pB->id<<":\n\tF="<<F<<", T="<<T; //"\n\tlocal F="<<C->phys->force*sign<<", T="<<C->phys->torque*sign<<"\n";
				throw std::runtime_error(oss.str().c_str());
			}
			if(watch.maxCoeff()==max(pA->id,pB->id) && watch.minCoeff()==min(pA->id,pB->id)){
				cerr<<"Step "<<scene->step<<": apply ##"<<pA->id<<"+"<<pB->id<<": F="<<F.transpose()<<", T="<<T.transpose()<<endl;
				cerr<<"\t#"<<(isPA?pA->id:pB->id)<<" @ "<<xc.transpose()<<", F="<<F.transpose()<<", T="<<(xc.cross(F)+T).transpose()<<endl;
		}
		#endif
		sh->nodes[0]->getData<DemData>().addForceTorque(F,xc.cross(F)+T);
	}
}


#ifdef WOO_OPENGL
WOO_PLUGIN(gl,(Gl1_Sphere));
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_Gl1_Sphere__CLASS_BASE_DOC_ATTRS);


#include"../supp/opengl/OpenGLWrapper.hpp"
#include"../supp/opengl/GLUtils.hpp"
#include"../gl/Renderer.hpp"
#include"../supp/base/CompUtils.hpp"

vector<Vector3r> Gl1_Sphere::vertices, Gl1_Sphere::faces;
int Gl1_Sphere::glStripedSphereList=-1;
int Gl1_Sphere::glGlutSphereList=-1;
Real  Gl1_Sphere::prevQuality=0;

// called for rendering spheres both and ellipsoids, differing in the scale parameter
void Gl1_Sphere::renderScaledSphere(const shared_ptr<Shape>& shape, const Vector3r& shift, bool wire2, const GLViewInfo& glInfo, const Real& radius, const Vector3r& scaleAxes){

	const shared_ptr<Node>& n=shape->nodes[0];
	Vector3r dPos=(n->hasData<GlData>()?n->getData<GlData>().dGlPos:Vector3r::Zero());
	GLUtils::setLocalCoords(shape->nodes[0]->pos+shift+dPos,shape->nodes[0]->ori);

	// for rendering ellipsoid
	if(!isnan(scaleAxes[0])) glScalev(scaleAxes);

	glClearDepth(1.0f);
	glEnable(GL_NORMALIZE);

	if(quality>10) quality=10; // insane setting can quickly kill the GPU

	Real r=radius*scale;
	//glColor3v(CompUtils::mapColor(shape->getBaseColor()));
	bool doPoints=(glInfo.renderer->fastDraw || quality<0 || (int)(quality*glutSlices)<2 || (int)(quality*glutStacks)<2);
	if(doPoints){
		if(smooth) glEnable(GL_POINT_SMOOTH);
		else glDisable(GL_POINT_SMOOTH);
		glPointSize(1.);
		glBegin(GL_POINTS);
			glVertex3v(Vector3r(0,0,0));
		glEnd();

	} else if (wire || wire2 ){
		glLineWidth(1.);
		if(!smooth) glDisable(GL_LINE_SMOOTH);
		glutWireSphere(r,quality*glutSlices,quality*glutStacks);
		if(!smooth) glEnable(GL_LINE_SMOOTH); // re-enable
	}
	else {
		glEnable(GL_LIGHTING);
		glShadeModel(GL_SMOOTH);
		glutSolidSphere(r,quality*glutSlices,quality*glutStacks);
	#if 0
		//Check if quality has been modified or if previous lists are invalidated (e.g. by creating a new qt view), then regenerate lists
		bool somethingChanged = (abs(quality-prevQuality)>0.001 || glIsList(glStripedSphereList)!=GL_TRUE);
		if (somethingChanged) {initStripedGlList(); initGlutGlList(); prevQuality=quality;}
		glScalef(r,r,r);
		if(stripes) glCallList(glStripedSphereList);
		else glCallList(glGlutSphereList);
	#endif
	}
	return;
}

void Gl1_Sphere::subdivideTriangle(Vector3r& v1,Vector3r& v2,Vector3r& v3, int depth){
	Vector3r v;
	//Change color only at the appropriate level, i.e. 8 times in total, since we draw 8 mono-color sectors one after another
	if (depth==int(quality) || quality<=0){
		v = (v1+v2+v3)/3.0;
		GLfloat matEmit[4];
		if (v[1]*v[0]*v[2]>0){
			matEmit[0] = 0.3;
			matEmit[1] = 0.3;
			matEmit[2] = 0.3;
			matEmit[3] = 1.f;
		}else{
			matEmit[0] = 0.15;
			matEmit[1] = 0.15;
			matEmit[2] = 0.15;
			matEmit[3] = 0.2;
		}
 		glMaterialfv(GL_FRONT, GL_EMISSION, matEmit);
	}
	if (depth==1){//Then display 4 triangles
		Vector3r v12 = v1+v2;
		Vector3r v23 = v2+v3;
		Vector3r v31 = v3+v1;
		v12.normalize();
		v23.normalize();
		v31.normalize();
		//Use TRIANGLE_STRIP for faster display of adjacent facets
		glBegin(GL_TRIANGLE_STRIP);
			glNormal3v(v1); glVertex3v(v1);
			glNormal3v(v31); glVertex3v(v31);
			glNormal3v(v12); glVertex3v(v12);
			glNormal3v(v23); glVertex3v(v23);
			glNormal3v(v2); glVertex3v(v2);
		glEnd();
		//terminate with this triangle left behind
		glBegin(GL_TRIANGLES);
			glNormal3v(v3); glVertex3v(v3);
			glNormal3v(v23); glVertex3v(v23);
			glNormal3v(v31); glVertex3v(v31);
		glEnd();
		return;
	}
	Vector3r v12 = v1+v2;
	Vector3r v23 = v2+v3;
	Vector3r v31 = v3+v1;
	v12.normalize();
	v23.normalize();
	v31.normalize();
	subdivideTriangle(v1,v12,v31,depth-1);
	subdivideTriangle(v2,v23,v12,depth-1);
	subdivideTriangle(v3,v31,v23,depth-1);
	subdivideTriangle(v12,v23,v31,depth-1);
}

void Gl1_Sphere::initStripedGlList() {
	if (!vertices.size()){//Fill vectors with vertices and facets
		//Define 6 points for +/- coordinates
		vertices.push_back(Vector3r(-1,0,0));//0
		vertices.push_back(Vector3r(1,0,0));//1
		vertices.push_back(Vector3r(0,-1,0));//2
		vertices.push_back(Vector3r(0,1,0));//3
		vertices.push_back(Vector3r(0,0,-1));//4
		vertices.push_back(Vector3r(0,0,1));//5
		//Define 8 sectors of the sphere
		faces.push_back(Vector3r(3,4,1));
		faces.push_back(Vector3r(3,0,4));
		faces.push_back(Vector3r(3,5,0));
		faces.push_back(Vector3r(3,1,5));
		faces.push_back(Vector3r(2,1,4));
		faces.push_back(Vector3r(2,4,0));
		faces.push_back(Vector3r(2,0,5));
		faces.push_back(Vector3r(2,5,1));
	}
	//Generate the list. Only once for each qtView, or more if quality is modified.
	glDeleteLists(glStripedSphereList,1);
	glStripedSphereList = glGenLists(1);
	glNewList(glStripedSphereList,GL_COMPILE);
	glEnable(GL_LIGHTING);
	glShadeModel(GL_SMOOTH);
	// render the sphere now
	for (int i=0;i<8;i++)
		subdivideTriangle(vertices[(unsigned int)faces[i][0]],vertices[(unsigned int)faces[i][1]],vertices[(unsigned int)faces[i][2]],1+ (int) quality);
	glEndList();

}

void Gl1_Sphere::initGlutGlList(){
	//Generate the "no-stripes" display list, each time quality is modified
	glDeleteLists(glGlutSphereList,1);
	glGlutSphereList = glGenLists(1);
	glNewList(glGlutSphereList,GL_COMPILE);
		glEnable(GL_LIGHTING);
		glShadeModel(GL_SMOOTH);
		glutSolidSphere(1.0,max(quality*glutSlices,2.),max(quality*glutStacks,3.));
	glEndList();
}

#endif
