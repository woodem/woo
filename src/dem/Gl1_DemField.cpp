#ifdef WOO_OPENGL
#include"Gl1_DemField.hpp"
#include"Sphere.hpp"
#include"Clump.hpp"
#include"InfCylinder.hpp"
#include"Wall.hpp"
#include"Facet.hpp"
#include"Funcs.hpp"
#include"../supp/opengl/GLUtils.hpp"
#include"../gl/Renderer.hpp"
#include"../supp/base/CompUtils.hpp"

#if 0 // defined(WOO_QT4) || defined(WOO_QT5)
	#include<QCoreApplication>
	// http://stackoverflow.com/questions/12201823/ipython-and-qt4-c-qcoreapplicationprocessevents-blocks
	#define PROCESS_GUI_EVENTS_SOMETIMES // { static int _i=0; if(guiEvery>0 && _i++>guiEvery){ _i=0; /*QCoreApplication::processEvents();*/ QCoreApplication::sendPostedEvents(); } }
#else
	#define PROCESS_GUI_EVENTS_SOMETIMES
#endif

WOO_PLUGIN(gl,
	(Gl1_DemField)
	(GlShapeFunctor)(GlShapeDispatcher)
	(GlBoundFunctor)(GlBoundDispatcher)
	(GlCPhysFunctor)(GlCPhysDispatcher)
	//(GlCGeomFunctor)(GlCGeomDispatcher)
);

WOO_IMPL_LOGGER(Gl1_DemField);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR_PY(woo_gl_Gl1_DemField__CLASS_BASE_DOC_ATTRS_CTOR_PY);


void Gl1_DemField::setFunctors_getRanges(const vector<shared_ptr<Object>>& ff, vector<shared_ptr<ScalarRange>>& rr){
	LOG_DEBUG("setting functors and ranges:");
	shapeDispatcher->setFunctors_getRanges(ff,rr);
	boundDispatcher->setFunctors_getRanges(ff,rr);
	cPhysDispatcher->setFunctors_getRanges(ff,rr);
	if(colorRanges.empty()) initAllRanges();
	for(const auto& r: colorRanges) if(r) rr.push_back(r);
	for(const auto& r: glyphRanges) if(r) rr.push_back(r);
}

void Gl1_DemField::initAllRanges(){
	for(int i=0; i<COLOR_SENTINEL; i++){
		auto r=make_shared<ScalarRange>();
		colorRanges.push_back(r);
		switch(i){
			case COLOR_SHAPE:	 		 r->label="Shape.color"; break;
			case COLOR_RADIUS:       r->label="radius"; break;
			case COLOR_DIAM_MM:      r->label="diameter (mm)"; break;
			case COLOR_VEL:          r->label="vel"; break;
			case COLOR_ANGVEL:       r->label="angVel"; break;
			case COLOR_MASS:         r->label="mass"; break;
			case COLOR_POS:          r->label="pos"; break;
			case COLOR_DISPLACEMENT: r->label="displacement"; break;
			case COLOR_ROTATION:     r->label="rotation"; break;
			case COLOR_REFPOS:       r->label="ref. pos"; break;
			case COLOR_MAT_ID:       r->label="material id"; break;
			case COLOR_MATSTATE:     r->label=""; break; // can only be filled from MatState::getScalarName(matStateIx)
			case COLOR_SIG_N: 	    r->label="normal stress"; break;
			case COLOR_SIG_T:    	 r->label="shear stress"; break;
			case COLOR_NUM_CON:    	 r->label="num. contacts"; break;
			case COLOR_FLAGS:    	 r->label="flags"; break;
			case COLOR_MASK:    		 r->label="mask"; break;
		};
	}
	colorRanges[COLOR_SHAPE]->setHidden(true); // don't show by default
	colorRange=colorRanges[colorBy];
	colorRange2=colorRanges[colorBy2];

	for(int i=0; i<GLYPH_SENTINEL; i++){
		if(i==GLYPH_NONE || i==GLYPH_KEEP){
			glyphRanges.push_back(shared_ptr<ScalarRange>());
			continue;
		}
		auto r=make_shared<ScalarRange>();
		glyphRanges.push_back(r);
		switch(i){
			case GLYPH_FORCE: r->label="force"; break;
			case GLYPH_TORQUE: r->label="torque"; break;
			case GLYPH_VEL:   r->label="velocity"; break;
			case GLYPH_ANGVEL:   r->label="angular velocity"; break;
		}
	}
	glyphRange=glyphRanges[glyph];
}

void Gl1_DemField::postLoad(Gl1_DemField&, void* attr){
	if(colorRanges.empty()) initAllRanges();
	if(attr==&matStateIx) colorRanges[COLOR_MATSTATE]->reset();
	// set to empty value so that it gets filled later
	if(colorBy==COLOR_MATSTATE) colorRange->label="";
	if(colorBy2==COLOR_MATSTATE) colorRange2->label="";

	colorRange=colorRanges[colorBy];
	colorRange2=colorRanges[colorBy2];
	glyphRange=glyphRanges[glyph];
}

void Gl1_DemField::doBound(){
	boundDispatcher->scene=scene; boundDispatcher->updateScenePtr();
	std::scoped_lock lock(dem->particles->manipMutex);
	for(const shared_ptr<Particle>& b: *dem->particles){
		// PROCESS_GUI_EVENTS_SOMETIMES; // rendering bounds is usually fast
		if(!b->shape || !b->shape->bound) continue;
		if(mask!=0 && !(b->mask&mask)) continue;
		glPushMatrix();
			(*boundDispatcher)(b->shape->bound);
		glPopMatrix();
	}
}

Vector3r Gl1_DemField::getParticleVel(const shared_ptr<Particle>& p) const {
	assert(p->shape);
	size_t nNodes=p->shape->nodes.size();
	if(nNodes==1){
		const auto& n0(p->shape->nodes[0]);
		const auto& dyn(n0->getData<DemData>());
		if(dyn.isClumped()) return getNodeVel(dyn.master.lock());
		else return getNodeVel(p->shape->nodes[0]);
	}
	Vector3r ret=Vector3r::Zero();
	for(const auto& n: p->shape->nodes) ret+=getNodeVel(n);
	return ret/nNodes;
}

Vector3r Gl1_DemField::getNodeVel(const shared_ptr<Node>& n) const{
	if(!scene->isPeriodic || !fluct) return n->getData<DemData>().vel;
	return scene->cell->pprevFluctVel(n->pos,n->getData<DemData>().vel,scene->dt);
}

Vector3r Gl1_DemField::getNodeAngVel(const shared_ptr<Node>& n) const{
	if(!scene->isPeriodic || !fluct) return n->getData<DemData>().angVel;
	return scene->cell->pprevFluctAngVel(n->getData<DemData>().angVel);
}

// this function is called for both rendering as well as
// in the selection mode

// nice reading on OpenGL selection
// http://glprogramming.com/red/chapter13.html

void Gl1_DemField::doShape(){
	const auto& renderer=viewInfo->renderer;
	shapeDispatcher->scene=scene; shapeDispatcher->updateScenePtr();
	std::scoped_lock lock(dem->particles->manipMutex);

	// experimental
	glHint(GL_POINT_SMOOTH_HINT, GL_FASTEST);

	// instead of const shared_ptr&, get proper shared_ptr;
	// Less efficient in terms of performance, since memory has to be written (not measured, though),
	// but it is still better than crashes if the body gets deleted meanwile.
	for(shared_ptr<Particle> p: *dem->particles){
		PROCESS_GUI_EVENTS_SOMETIMES;

		if(!p->shape || p->shape->nodes.empty()) continue;

		const shared_ptr<Shape>& sh=p->shape;

		// sets highlighted color, if the particle is selected
		// last optional arg can be used to provide additional highlight conditions (unused for now)
		const shared_ptr<Node>& _n0=p->shape->nodes[0];
		const shared_ptr<Node>& n0=(_n0->getData<DemData>().isClumped()?_n0->getData<DemData>().master.lock():_n0);
		Renderer::glScopedName name(*viewInfo,p,n0,/*highLev=*/(p->shape->getHighlighted()?0:-1));
		const auto& dyn0=n0->getData<DemData>();
		// bool highlight=(p->id==selId || (p->clumpId>=0 && p->clumpId==selId) || p->shape->highlight);

		// if any of the particle's nodes is clipped, don't display it at all
		bool clipped=false;
		for(const shared_ptr<Node>& n: p->shape->nodes){
			renderer->setNodeGlData(n);
			if(n->getData<GlData>().isClipped()) clipped=true;
		}

		if(!sh->getVisible() || clipped) continue;

		bool useColor2=false;
		bool isSpheroid(!isnan(p->shape->equivRadius()));
		Real radius=p->shape->equivRadius();
		if(dyn0.isClump()) radius=dyn0.cast<ClumpData>().equivRad;

		switch(shape){
			case SHAPE_NONE: useColor2=true; break;
			case SHAPE_ALL: useColor2=false; break;
			case SHAPE_SPHERES: useColor2=!isSpheroid; break;
			case SHAPE_NONSPHERES: useColor2=isSpheroid; break;
			case SHAPE_MASK: useColor2=(mask!=0 && !(p->mask & mask)); break;
			default: useColor2=true; break;  // invalid value, filter not matching
		}
		// additional conditions under which colorBy2 is used
		if(false
			|| (colorBy==COLOR_RADIUS && isnan(radius))
			|| (colorBy==COLOR_DIAM_MM && isnan(radius))
			|| (colorBy==COLOR_MATSTATE && !p->matState)
			/* || (!isSpheroid && (colorBy==COLOR_SIG_N || colorBy==COLOR_SIG_T)) */
		) useColor2=true;

		// don't show particles not matching modulo value
		if(!useColor2 && modulo[0]>0 && (p->id+modulo[1])%modulo[0]!=0) continue;


		if(!shape2 && useColor2) continue; // skip particle

		Vector3r parColor;
		
		// choose colorRange for colorBy or colorBy2
		const shared_ptr<ScalarRange>& CR(!useColor2?colorRange:colorRanges[colorBy2]);
		auto vecNormXyz=[&](const Vector3r& v)->Real{
			switch(vecAxis){
				case 0: case 1: case 2: return v[vecAxis];
				case AXIS_YZ: return v.tail<2>().norm();
				case AXIS_ZX: return sqrt(pow2(v[2])+pow2(v[1]));
				case AXIS_XY: return v.head<2>().norm();
				default: return v.norm();
			}
			// if(useColor2 || vecAxis<0||vecAxis>2) return v.norm(); return v[vecAxis];
		};
		int cBy=(!useColor2?colorBy:colorBy2);
		switch(cBy){
			case COLOR_RADIUS: parColor=(!isnan(radius)?CR->color(radius):(solidColor)); break;
			case COLOR_DIAM_MM: parColor=(!isnan(radius)?CR->color(2000*radius):(solidColor)); break;
			case COLOR_VEL: parColor=CR->color(vecNormXyz(getParticleVel(p))); break;
			case COLOR_ANGVEL: parColor=CR->color(vecNormXyz(getNodeAngVel(n0))); break;
			case COLOR_MASS: parColor=CR->color(dyn0.mass); break;
			case COLOR_POS: parColor=CR->color(vecNormXyz(n0->pos)); break;
			case COLOR_DISPLACEMENT: parColor=CR->color(vecNormXyz(n0->pos-n0->getData<GlData>().refPos)); break;
			case COLOR_ROTATION: {
				AngleAxisr aa(n0->ori.conjugate()*n0->getData<GlData>().refOri);
				parColor=CR->color(vecAxis==AXIS_NORM?aa.angle():vecNormXyz(aa.angle()*aa.axis()));
				break;
			}
			case COLOR_REFPOS: parColor=CR->color(vecNormXyz(n0->getData<GlData>().refPos)); break;
			case COLOR_MAT_ID: parColor=CR->color(p->material->id); break;
			case COLOR_MATSTATE:{
				if(CR->label.empty() && p->matState) CR->label=p->matState->getScalarName(matStateIx);
				Real sc=(p->matState?p->matState->getScalar(matStateIx,scene->time,scene->step,matStateSmooth):NaN);
				parColor=isnan(sc)?solidColor:CR->color(sc);
				break;
			}
			case COLOR_SHAPE: parColor=CR->color(p->shape->getBaseColor()); break;
			case COLOR_SOLID: parColor=solidColor; break;
			case COLOR_SIG_N:
			case COLOR_SIG_T: {
				Vector3r sigN, sigT;
				bool sigOk=DemFuncs::particleStress(p,sigN,sigT);
				if(sigOk) parColor=CR->color(vecNormXyz(cBy==COLOR_SIG_N?sigN:sigT));
				else parColor=solidColor;
				break;
			}
			case COLOR_MASK: parColor=CR->color(p->mask); break;
			case COLOR_NUM_CON: parColor=CR->color(p->countRealContacts()); break;
			case COLOR_FLAGS: parColor=CR->color(dyn0.flags); break;
			case COLOR_INVISIBLE: continue; // don't show this particle at all
			default: parColor=Vector3r(NaN,NaN,NaN);
		}

		// particle not rendered
		if(isnan(parColor.maxCoeff())) continue;
		
		// fast-track for spheres (don't call the functor, avoid glPushMatrix())
		if(renderer->fastDraw && p->shape->isA<Sphere>()){
			glColor3v(parColor);
			glBegin(GL_POINTS);
				glVertex3v((n0->pos+n0->getData<GlData>().dGlPos).eval());
			glEnd();
		} else {
			glPushMatrix();
				glColor3v(parColor);
				(*shapeDispatcher)(p->shape,/*shift*/Vector3r::Zero(),wire||sh->getWire(),*viewInfo);
			glPopMatrix();
		}

		if(name.highlighted){
			const Vector3r& pos=sh->nodes[0]->pos;
			if(!sh->bound || wire || sh->getWire()) GLUtils::GLDrawInt(p->id,pos);
			else {
				// move the label towards the camera by the bounding box so that it is not hidden inside the body
				const Vector3r& mn=sh->bound->min; const Vector3r& mx=sh->bound->max;
				Vector3r ext(renderer->viewDirection[0]>0?pos[0]-mn[0]:pos[0]-mx[0],renderer->viewDirection[1]>0?pos[1]-mn[1]:pos[1]-mx[1],renderer->viewDirection[2]>0?pos[2]-mn[2]:pos[2]-mx[2]); // signed extents towards the camera
				Vector3r dr=-1.01*(renderer->viewDirection.dot(ext)*renderer->viewDirection);
				GLUtils::GLDrawInt(p->id,pos+dr,Vector3r::Ones());
			}
		}
		// if the body goes over the cell margin, draw it in positions where the bbox overlaps with the cell in wire
		// precondition: pos is inside the cell.
		if(sh->bound && scene->isPeriodic && renderer->ghosts){
			const Vector3r& cellSize(scene->cell->getSize());
			//Vector3r pos=scene->cell->unshearPt(.5*(sh->bound->min+sh->bound->max)); // middle of bbox, remove the shear component
			Vector3r pos=.5*(sh->bound->min+sh->bound->max); // middle of bbox, in sheared coords already
			pos-=scene->cell->intrShiftPos(sh->nodes[0]->getData<GlData>().dCellDist); // wrap the same as node[0] was wrapped
			// traverse all periodic cells around the body, to see if any of them touches
			Vector3r halfSize=.5*(sh->bound->max-sh->bound->min);
			// handle infinite bboxes: set position to the middle of the cell
			// this will not work with sheared cell, but we don't support that with inifinite bounds anyway
			for(int ax:{0,1,2}){
				const auto& cellDim(scene->cell->getHSize().diagonal());
				if(isinf(sh->bound->min[ax]) || isinf(sh->bound->max[ax])){
					pos[ax]=halfSize[ax]=.5*cellDim[ax];
				}
			}
			Vector3r pmin,pmax;
			Vector3i i;
			// try all positions around, to see if any of them sticks outside the cell boundary
			for(i[0]=-1; i[0]<=1; i[0]++) for(i[1]=-1;i[1]<=1; i[1]++) for(i[2]=-1; i[2]<=1; i[2]++){
				if(i[0]==0 && i[1]==0 && i[2]==0) continue; // middle; already rendered above
				Vector3r pos2=pos+Vector3r(cellSize[0]*i[0],cellSize[1]*i[1],cellSize[2]*i[2]); // shift, but without shear!
				pmin=pos2-halfSize; pmax=pos2+halfSize;
				if(pmin[0]<=cellSize[0] && pmax[0]>=0 &&
					pmin[1]<=cellSize[1] && pmax[1]>=0 &&
					pmin[2]<=cellSize[2] && pmax[2]>=0) {
					Vector3r pt=scene->cell->shearPt(pos2);
					// if(pointClipped(pt)) continue;
					glPushMatrix();
						(*shapeDispatcher)(p->shape,/*shift*/pt-pos,/*wire*/true,*viewInfo);
					glPopMatrix();
				}
			}
		}
	}
}

void Gl1_DemField::doNodes(const vector<shared_ptr<Node>>& nodeContainer){
	std::scoped_lock lock(dem->nodesMutex);
	// not sure if this is right...?
	glDisable(GL_LIGHTING);

	viewInfo->renderer->nodeDispatcher.scene=scene; viewInfo->renderer->nodeDispatcher.updateScenePtr();
	for(shared_ptr<Node> n: nodeContainer){
		PROCESS_GUI_EVENTS_SOMETIMES;

		viewInfo->renderer->setNodeGlData(n);
		if(n->getData<GlData>().isClipped()) continue;
		const DemData& dyn=n->getData<DemData>();

		// don't show particles not matching modulo value on first attached particle's id
		if(modulo[0]>0 && !dyn.parRef.empty() && (((*dyn.parRef.begin())->id+modulo[1])%modulo[0])!=0) continue;

		if(glyph!=GLYPH_KEEP){
			// prepare rep types
			// vector values
			if(glyph==GLYPH_VEL || glyph==GLYPH_ANGVEL || glyph==GLYPH_FORCE || glyph==GLYPH_TORQUE){
				if(!n->rep || !n->rep->isA<VectorGlRep>()){ n->rep=make_shared<VectorGlRep>(); }
				auto& vec=n->rep->cast<VectorGlRep>();
				vec.range=glyphRange;
				vec.relSz=glyphRelSz;
			}
			switch(glyph){
				case GLYPH_NONE: n->rep.reset(); break; // no rep
				case GLYPH_VEL: n->rep->cast<VectorGlRep>().val=getNodeVel(n); break;
				case GLYPH_ANGVEL: n->rep->cast<VectorGlRep>().val=dyn.angVel; break;
				case GLYPH_FORCE: n->rep->cast<VectorGlRep>().val=dyn.force; break;
				case GLYPH_TORQUE: n->rep->cast<VectorGlRep>().val=dyn.torque; break;
				case GLYPH_KEEP:
				default: ;
			}
		}

		if(!nodes && !n->rep) continue;

		Renderer::glScopedName name(*viewInfo,n);
		if(nodes){ viewInfo->renderer->renderRawNode(n); }
		if(n->rep){ n->rep->render(n,viewInfo); }
	}
}


void Gl1_DemField::doContactNodes(){
	// if(cNode==CNODE_NONE) return;
	viewInfo->renderer->nodeDispatcher.scene=scene; viewInfo->renderer->nodeDispatcher.updateScenePtr();
	std::scoped_lock lock(dem->contacts->manipMutex);
	for(size_t i=0; i<dem->contacts->size(); i++){
		PROCESS_GUI_EVENTS_SOMETIMES;
		const shared_ptr<Contact>& C((*dem->contacts)[i]);
		const Particle *pA=C->leakPA(), *pB=C->leakPB();
		if(C->isReal()){
			shared_ptr<CGeom> geom=C->geom;
			if(!geom) continue;
			shared_ptr<Node> node=geom->node;
			// ensure dGlPos is defined
			for(const Particle* p: {pA,pB}) if(!p->shape->nodes[0]->hasData<GlData>()) viewInfo->renderer->setNodeGlData(p->shape->nodes[0]);
			viewInfo->renderer->setNodeGlData(node);
			Renderer::glScopedName name(*viewInfo,C,node);
			if((cNode & CNODE_GLREP) && node->rep){ node->rep->render(node,viewInfo); }
			if(cNode & CNODE_LINE){
				assert(pA->shape && pB->shape);
				assert(pA->shape->nodes.size()>0); assert(pB->shape->nodes.size()>0);
				Vector3r x[3]={node->pos,pA->shape->avgNodePos(),pB->shape->avgNodePos()};
				const Vector3r& dx0(node->getData<GlData>().dGlPos);
				const Vector3r& dx1(pA->shape->nodes[0]->getData<GlData>().dGlPos);
				const Vector3r& dx2(pB->shape->nodes[0]->getData<GlData>().dGlPos);
				if(scene->isPeriodic){
					Vector3i cellDist;
					x[0]=scene->cell->canonicalizePt(x[0],cellDist);
					x[1]+=scene->cell->intrShiftPos(-cellDist);
					x[2]+=scene->cell->intrShiftPos(-cellDist+C->cellDist);
				}
				Vector3r color=CompUtils::mapColor(C->color);
				if(isnan(color.maxCoeff())) continue;
				if(pA->shape->isA<Sphere>()) GLUtils::GLDrawLine(x[0]+dx0,x[1]+dx1,color);
				GLUtils::GLDrawLine(x[0]+dx0,x[2]+dx2,color);
			}
			if(cNode & CNODE_NODE) viewInfo->renderer->renderRawNode(node);
		} else {
			if(!(cNode & CNODE_POTLINE)) continue;
			assert(pA->shape && pB->shape);
			assert(pA->shape->nodes.size()>0); assert(pB->shape->nodes.size()>0);
			for(const Particle* p: {pA,pB}) if(!p->shape->nodes[0]->hasData<GlData>()) viewInfo->renderer->setNodeGlData(p->shape->nodes[0]);
			Vector3r A; Vector3r B=pB->shape->avgNodePos();
			if(pA->shape->isA<Sphere>()) A=pA->shape->nodes[0]->pos;
			else if(pA->shape->isA<Wall>()){ A=pA->shape->nodes[0]->pos; int ax=pA->shape->cast<Wall>().axis; A[(ax+1)%3]=B[(ax+1)%3]; A[(ax+2)%3]=B[(ax+2)%3]; }
			else if(pA->shape->isA<InfCylinder>()){ A=pA->shape->nodes[0]->pos; int ax=pA->shape->cast<InfCylinder>().axis; A[ax]=B[ax]; }
			else if(pA->shape->isA<Facet>()){ A=pA->shape->cast<Facet>().getNearestPt(B); }
			else A=pA->shape->avgNodePos();
			const Vector3r& dA(pA->shape->nodes[0]->getData<GlData>().dGlPos); const Vector3r& dB(pB->shape->nodes[0]->getData<GlData>().dGlPos);
			// if(scene->isPeriodic){ B+=scene->cell->intrShiftPos(C->cellDist); }
			GLUtils::GLDrawLine(A+dA,B+dB,.5*CompUtils::mapColor(C->color));
		}
	}
}


void Gl1_DemField::doCPhys(){
	glEnable(GL_LIGHTING);
	cPhysDispatcher->scene=scene; cPhysDispatcher->updateScenePtr();
	std::scoped_lock lock(dem->contacts->manipMutex);
	for(const shared_ptr<Contact>& C: *dem->contacts){
		PROCESS_GUI_EVENTS_SOMETIMES;
		#if 1
			shared_ptr<CGeom> geom(C->geom);
			shared_ptr<CPhys> phys(C->phys);
		#else
			// HACK: make fast now
			shared_ptr<CGeom>& geom(C->geom);
			shared_ptr<CPhys>& phys(C->phys);
		#endif
		//assert(C->leakPA()->shape && C->leakPB()->shape);
		//assert(C->leakPA()->shape->nodes.size()>0); assert(C->leakPB()->shape->nodes.size()>0);
		if(!geom || !phys) continue;
		viewInfo->renderer->setNodeGlData(geom->node);
		if(geom->node->getData<GlData>().isClipped()) continue;
		Renderer::glScopedName name(*viewInfo,C,geom->node);
		(*cPhysDispatcher)(phys,C,*viewInfo);
	}
}





void Gl1_DemField::go(const shared_ptr<Field>& demField, GLViewInfo* _viewInfo){
	dem=static_pointer_cast<DemField>(demField);
	viewInfo=_viewInfo;
	if(colorRanges.empty()) initAllRanges();

	//if(doPostLoad || _lastScene!=scene) postLoad2();
	//doPostLoad=false; _lastScene=scene;
	periodic=scene->isPeriodic;
	if(viewInfo->renderer->setRefNow && periodic) scene->cell->refHSize=scene->cell->hSize;

	if(shape!=SHAPE_NONE || shape2) doShape();

	if(!viewInfo->renderer->fastDraw){
		if(bound) doBound();
		doNodes(dem->nodes);
		if(deadNodes && !dem->deadNodes.empty()) doNodes(dem->deadNodes);
		if(cNode!=CNODE_NONE) doContactNodes();
		if(cPhys) doCPhys();
	}
};

#endif


