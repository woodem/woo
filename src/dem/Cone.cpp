#include<woo/pkg/dem/Cone.hpp>

WOO_PLUGIN(dem,(Cone)(Bo1_Cone_Aabb)(Cg2_Cone_Sphere_L6Geom));

WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR(woo_dem_Cone__CLASS_BASE_DOC_ATTRS_CTOR);
WOO_IMPL__CLASS_BASE_DOC(woo_dem_Cg2_Cone_Sphere_L6Geom__CLASS_BASE_DOC);
WOO_IMPL__CLASS_BASE_DOC(woo_dem_Bo1_Cone_Aabb__CLASS_BASE_DOC);

void Bo1_Cone_Aabb::go(const shared_ptr<Shape>& sh){
	assert(sh->numNodesOk());
	if(scene->isPeriodic && scene->cell->hasShear()) throw std::runtime_error("Bo1_Cone_Aabb::go: sheared periodic boundaries for Cone not (yet) implemented.");
	Cone& c=sh->cast<Cone>();
	if(!c.bound){ c.bound=make_shared<Aabb>(); /* ignore node rotation*/ c.bound->cast<Aabb>().maxRot=-1; }
	Aabb& aabb=sh->bound->cast<Aabb>();
	const Vector3r axUnit=Vector3r(c.nodes[1]->pos-c.nodes[0]->pos).normalized();
	for(short ax: {0,1,2}){
		for(short end: {0,1}){
			const Real& r(c.radii[end]);
			Real dAx=c.radii[end]*axUnit.cross(Vector3r::Unit(ax)).norm();
			for(short dir:{-1,1}){
				Real P=c.nodes[end]->pos[ax]+dir*dAx;
				if(end==0 && dir==-1) aabb.min[ax]=aabb.max[ax]=P;
				else {
					aabb.min[ax]=min(P,aabb.min[ax]);
					aabb.max[ax]=max(P,aabb.max[ax]);
				}
			}
		}
	}
};

WOO_IMPL_LOGGER(Cg2_Cone_Sphere_L6Geom);

bool Cg2_Cone_Sphere_L6Geom::go(const shared_ptr<Shape>& s1, const shared_ptr<Shape>& s2, const Vector3r& shift2, const bool& force, const shared_ptr<Contact>& C){
	const Cone& c=s1->cast<Cone>(); const Sphere& s=s2->cast<Sphere>();
	assert(s1->numNodesOk()); assert(s2->numNodesOk());
	assert(s1->nodes[0]->hasData<DemData>()); assert(s2->nodes[0]->hasData<DemData>());
	const DemData& dynCone(c.nodes[0]->getData<DemData>());
	const DemData& dynSphere(s.nodes[0]->getData<DemData>());
	const Vector3r& A(c.nodes[0]->pos);
	const Vector3r& B(c.nodes[1]->pos);
	Vector3r S=s.nodes[0]->pos+shift2;
	Vector3r AB1=(B-A).normalized();
	// perpendicular to the axis-sphere plane (x-perp)
	Vector3r perpX1=((S-A).cross(B-A)).normalized();
	// perpendicular to the axis, in the axis-sphere plane
	Vector3r perpA1=AB1.cross(perpX1); // both normalized and perpendicular; should be also normalized
	
	Vector3r contPt;
	Real coneContRad; // cone radius at the contact point
	bool inside=false; // sphere center is inside the cone

	// for degenerate case (exactly on the axis), the norm will be zero
	if(perpX1.squaredNorm()>0){ // OK
		Vector3r P=A+perpA1*c.radii[0], Q=B+perpA1*c.radii[1];
		Real normSidePos;
		Vector3r sidePt=CompUtils::closestSegmentPt(S,P,Q,&normSidePos);
		// radius interpolated along the side
		coneContRad=(1-normSidePos)*c.radii[0]+normSidePos*c.radii[1];
		//LOG_DEBUG("normSidePos={}",normSidePos);
		// touches rim or base
		if(normSidePos==0 || normSidePos==1){
			Vector3r E(normSidePos==0?A:B);
			Real axDist=(S-E).dot(perpA1);
			// touches the rim
			if(axDist>=c.radii[normSidePos==0?0:1]) contPt=sidePt;
			// touches the base
			else contPt=E+perpA1*axDist;
			short dir=(normSidePos==0?-1:1);
			if((S-E).dot(AB1*dir)<0) inside=true;
		}
		// touches the side
		else {
			Vector3r outerVec=(Q-P).cross(perpX1);
			if((S-sidePt).dot(outerVec)<=0) inside=true;
			//LOG_DEBUG("inside={},S-sidePt={},outerVec={},(S-sidePt).dot(outerVec)={}",inside,(S-sidePt).transpose(),outerVec.transpose(),(S-sidePt).dot(outerVec));
			contPt=sidePt;
		}
	}
	// degenerate case
	else {
		LOG_ERROR("Degenerate cone-sphere intersection (exactly on the axis) not handled yet!");
		throw std::runtime_error("Degenerate cone-sphere intersection.");
	}
	Real unDistSq=(S-contPt).squaredNorm()-pow2(s.radius);

	if(!inside && unDistSq>0 && !C->isReal() && !force) return false;

	Real uN=(inside?-1:1)*(S-contPt).norm()-s.radius;
	Vector3r normal=(inside?-1:1)*(S-contPt).normalized();
	Vector3r coneAxisPt=A+AB1*AB1.dot(contPt);

	// LOG_INFO("inside={},uN={},S={},contPt={}",inside,uN,S.transpose(),contPt.transpose());

	handleSpheresLikeContact(C,coneAxisPt,dynCone.vel,dynCone.angVel,S,dynSphere.vel,dynSphere.angVel,normal,contPt,uN,coneContRad,s.radius);

	return true;
};


#ifdef WOO_OPENGL
WOO_PLUGIN(gl,(Gl1_Cone));
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_Gl1_Cone__CLASS_BASE_DOC_ATTRS);

#include<woo/lib/opengl/OpenGLWrapper.hpp>
#include<woo/lib/opengl/GLUtils.hpp>
#include<woo/lib/base/CompUtils.hpp>
#include<woo/pkg/gl/GlData.hpp>
void Gl1_Cone::go(const shared_ptr<Shape>& shape, const Vector3r& shift, bool wire2, const GLViewInfo&){
	const Cone& c(shape->cast<Cone>());
	assert(c.numNodesOk());
	glShadeModel(GL_SMOOTH);
	Vector3r shifts[2]={shift,shift};
	if(scene->isPeriodic && c.nodes[0]->hasData<GlData>() && c.nodes[1]->hasData<GlData>()){
		GlData* g[2]={&(c.nodes[0]->getData<GlData>()),&(c.nodes[1]->getData<GlData>())};
		if(g[0]->dCellDist!=g[1]->dCellDist){
			Vector3i dCell;
			scene->cell->canonicalizePt(.5*(c.nodes[0]->pos+c.nodes[1]->pos),dCell);
			for(int i:{0,1}) shifts[i]+=scene->cell->intrShiftPos(g[i]->dCellDist-dCell);
		}
	}
	// draw endpoints with displacement scaled (glA,glB), but compute rotation from the real endpoints
	Vector3r glA=shifts[0]+c.nodes[0]->pos+(c.nodes[0]->hasData<GlData>()?c.nodes[0]->getData<GlData>().dGlPos:Vector3r::Zero());
	Vector3r glB=shifts[1]+c.nodes[1]->pos+(c.nodes[1]->hasData<GlData>()?c.nodes[1]->getData<GlData>().dGlPos:Vector3r::Zero());

	// real positions
	const Vector3r& A(c.nodes[0]->pos); const Vector3r& B(c.nodes[1]->pos);
	Vector3r AB=(B-A); Real lAB=AB.norm(); Vector3r AB1=AB/lAB;

	// axial orientation from node positions
	Quaternionr ori=Quaternionr::FromTwoVectors(Vector3r::UnitX(),glB-glA);
	// axis rotation from node 0 rotation, but just the part along the axis
	AngleAxisr aa(c.nodes[0]->ori);
	Real a2=aa.angle()*(aa.axis().dot(AB1));
	GLUtils::setLocalCoords(A,ori*AngleAxis(a2,Vector3r::UnitX()));

	GLUtils::Cylinder(Vector3r::Zero(),Vector3r((glB-glA).norm(),0,0),c.radii[0],/*color: keep current*/Vector3r(-1,-1,-1),c.getWire()||wire||wire2,/*caps*/true,c.radii[1],slices,stacks);
}
#endif



