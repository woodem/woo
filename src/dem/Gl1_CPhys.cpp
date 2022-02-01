
#ifdef WOO_OPENGL

#include"Gl1_CPhys.hpp"
#include"../gl/Renderer.hpp" // for GlData
#include"../supp/opengl/GLUtils.hpp"
#include"Sphere.hpp"

WOO_PLUGIN(gl,(Gl1_CPhys));
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_Gl1_CPhys__CLASS_BASE_DOC_ATTRS);

void Gl1_CPhys::go(const shared_ptr<CPhys>& cp, const shared_ptr<Contact>& C, const GLViewInfo& viewInfo){
	if(!range) return;
	if(shearColor && !shearRange) shearColor=false;
	// shared_ptr<CGeom> cg(C->geom); if(!cg) return; // changed meanwhile?
	Real fn=cp->force[0];
	if(isnan(fn)) return;
	if((signFilter>0 && fn<0) || (signFilter<0 && fn>0)) return;
	range->norm(fn); // adjust range, return value discarded
	Real r=relMaxRad*viewInfo.sceneRadius*min(1.,(abs(fn)/(max(abs(range->mnmx[0]),abs(range->mnmx[1])))));
	if(r<viewInfo.sceneRadius*1e-4 || isnan(r)) return;
	Vector3r color=shearColor?shearRange->color(Vector2r(cp->force[1],cp->force[2]).norm()):range->color(fn);
	if(isnan(color.maxCoeff())) return;
	const Particle *pA=C->leakPA(), *pB=C->leakPB();
	// get both particle's space positions, but B moved so that it is relative to A as the contact is
	Vector3r A=(pA->shape->isA<Sphere>()?pA->shape->nodes[0]->pos:C->geom->node->pos);
	Vector3r B=pB->shape->avgNodePos()+((scene->isPeriodic)?(scene->cell->intrShiftPos(C->cellDist)):Vector3r::Zero());
	if(pA->shape->nodes[0]->hasData<GlData>() && pB->shape->nodes[0]->hasData<GlData>()){
		const GlData &glA=pA->shape->nodes[0]->getData<GlData>(), &glB=pB->shape->nodes[0]->getData<GlData>();
		// move particles so that they are as they would be displayed (A is the master, B may be off if necessary for the contact, as per intrShiftPos above)
		A+=glA.dGlPos; B+=glB.dGlPos;
		// if points were wrapped by different number of cells in dGlPos, fix that
		if(scene->isPeriodic && glA.dCellDist!=glB.dCellDist){
			// move B by its position difference, but by as many cells as A
			// so move it back by glB.dCellDist and forward by glA.dCellDist
			// then they should be positioned relatively as they should be for the contact
			// adjacent to the position where "A" is displayed as particle
			B+=scene->cell->intrShiftPos(glB.dCellDist-glA.dCellDist);
		}
	}
	// cerr<<"["<<r<<"]";
	GLUtils::Cylinder(A,B,r,color,/*wire*/false,/*caps*/false,/*rad2*/-1,slices);
}
#endif /* WOO_OPENGL */
