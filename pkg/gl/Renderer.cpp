#ifdef WOO_OPENGL

#include<woo/pkg/gl/Renderer.hpp>
#include<woo/lib/opengl/OpenGLWrapper.hpp>
#include<woo/lib/opengl/GLUtils.hpp>
#include<woo/lib/base/CompUtils.hpp>
#include<woo/lib/pyutil/gil.hpp>
#include<woo/core/Timing.hpp>
#include<woo/core/Scene.hpp>
#include<woo/core/Field.hpp>

#include<woo/pkg/gl/GlWooLogo.hpp>
#include<woo/pkg/gl/GlSetup.hpp>

// #include<woo/pkg/dem/Particle.hpp>

#include <GL/glu.h>
#include <GL/gl.h>
#include <GL/glut.h>

WOO_PLUGIN(gl,(Renderer)(GlExtraDrawer)(GlExtra_EnergyTrackerGrid));
WOO_IMPL_LOGGER(Renderer);

WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_gl_GlExtraDrawer__CLASS_BASE_DOC_ATTRS_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_gl_GlExtra_EnergyTrackerGrid__CLASS_BASE_DOC_ATTRS);

WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR_PY(woo_gl_Renderer__CLASS_BASE_DOC_ATTRS_CTOR_PY);



// declaration in core/Scene.hpp (!!!)
void Scene::ensureGl(){ if(!gl){ gl=make_shared<GlSetup>(); gl->init(); } }
shared_ptr<Renderer> Scene::ensureAndGetRenderer() { ensureGl(); return gl->getRenderer(); }
shared_ptr<Object> Scene::pyEnsureAndGetRenderer(){ return ensureAndGetRenderer(); }
shared_ptr<GlSetup> Scene::pyGetGl(){ ensureGl(); return gl; }
void Scene::pySetGl(const shared_ptr<GlSetup>& g){ if(!g) throw std::runtime_error("Scene.gl: must not be None."); gl=g; glDirty=true; }


void GlExtraDrawer::render(const GLViewInfo& viewInfo){ throw runtime_error("GlExtraDrawer::render called from class "+getClassName()+". (did you forget to override it in the derived class?)"); }

void GlExtra_EnergyTrackerGrid::render(const GLViewInfo& viewInfo){
	const Scene* scene(viewInfo.scene);
	if(!scene->trackEnergy) return;
	if(!scene->energy->grid) return;
	const auto& g(scene->energy->grid);
	if(viewInfo.renderer->fastDraw) GLUtils::AlignedBox(g->box,color);
	else GLUtils::AlignedBoxWithTicks(g->box,Vector3r::Constant(g->cellSize),Vector3r::Constant(g->cellSize),color);
}


Vector3r Renderer::highlightEmission0;
Vector3r Renderer::highlightEmission1;

void Renderer::postLoad(Renderer&, void* attr){
	if(attr==&dispScale && dispScale!=Vector3r::Ones()) scaleOn=true;
	if(attr==&rotScale && rotScale!=1.) scaleOn=true;
	if(clipPlanes.empty()) clipPlanes.resize(numClipPlanes);
	// don't allow None clipping plane
	for(size_t i=0; i<clipPlanes.size(); i++) if(!clipPlanes[i]) clipPlanes[i]=make_shared<Node>();
}

void Renderer::init(const shared_ptr<Scene>& scene){
	LOG_DEBUG("Renderer::init()");
	if(!scene->gl) throw std::logic_error("Renderer::init: !scene->gl.");
	if(scene->gl->getRenderer().get()!=this) throw std::logic_error("Renderer::init: scene->gl->getRenderer().get()!=this.");
	scene->autoRanges.clear();
	fieldDispatcher.setFunctors_getRanges(scene->gl->objs,scene->autoRanges);
	nodeDispatcher.setFunctors_getRanges(scene->gl->objs,scene->autoRanges);
	for(const auto& e: scene->engines) e->getRanges(scene->autoRanges);

	initDone=true;

	static bool glutInitDone=false;
	if(!glutInitDone){
		char* argv[]={NULL};
		int argc=0;
		glutInit(&argc,argv);
		//glutInitDisplayMode(GLUT_DOUBLE);
		/* transparent spheres (still not working): glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_MULTISAMPLE | GLUT_ALPHA); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); */
		glutInitDone=true;
	}
}

bool Renderer::pointClipped(const Vector3r& p){
	if(clipPlanes.empty()) return false;
	for(const auto& cp: clipPlanes) if(cp->rep && cp->glob2loc(p).z()<0.) return true;
	return false;
}

const Vector3r& Renderer::axisColor(short ax){
	return (ax==0?colorX:(ax==1?colorY:colorZ));
}



/* this function is called from field renderers for all field nodes */
void Renderer::setNodeGlData(const shared_ptr<Node>& n){
	bool scaleRotations=(rotScale!=1.0 && scaleOn);
	bool scaleDisplacements=(dispScale!=Vector3r::Ones() && scaleOn);
	const bool isPeriodic=scene->isPeriodic;

	if(!n->hasData<GlData>()) n->setData<GlData>(make_shared<GlData>());
	GlData& gld=n->getData<GlData>();
	// inside the cell if periodic, same as pos otherwise
	Vector3r cellPos=(!isPeriodic ? n->pos : scene->cell->canonicalizePt(n->pos,gld.dCellDist)); 
	bool rendered=!pointClipped(cellPos);
	// this encodes that the node is clipped
	if(!rendered){ gld.dGlPos=Vector3r(NaN,NaN,NaN); return; }
	if(setRefNow || isnan(gld.refPos[0])) gld.refPos=n->pos;
	if(setRefNow || isnan(gld.refOri.x())) gld.refOri=n->ori;
	const Vector3r& pos=n->pos; const Vector3r& refPos=gld.refPos;
	const Quaternionr& ori=n->ori; const Quaternionr& refOri=gld.refOri;
	// if no scaling and no periodic, return quickly
	if(!(scaleDisplacements||scaleRotations||isPeriodic)){ gld.dGlPos=Vector3r::Zero(); gld.dGlOri=Quaternionr::Identity(); return; }
	// apply scaling
	gld.dGlPos=cellPos-n->pos;
	// add scaled translation to the point of reference
	if(scaleDisplacements) gld.dGlPos+=((dispScale-Vector3r::Ones()).array()*Vector3r(pos-refPos).array()).matrix(); 
	if(!scaleRotations) gld.dGlOri=Quaternionr::Identity();
	else{
		Quaternionr relRot=refOri.conjugate()*ori;
		AngleAxisr aa(relRot);
		aa.angle()*=rotScale;
		gld.dGlOri=Quaternionr(aa);
	}
}

// draw periodic cell, if active
void Renderer::drawPeriodicCell(){
	if(!scene->isPeriodic || !cell) return;
	glColor3v(Vector3r(1,1,0));
	glPushMatrix();
		const Matrix3r& hSize=scene->cell->hSize;
		if(scaleOn && dispScale!=Vector3r::Ones()){
			const Matrix3r& refHSize(scene->cell->refHSize);
			Matrix3r scaledHSize;
			for(int i=0; i<3; i++) scaledHSize.col(i)=refHSize.col(i)+((dispScale-Vector3r::Ones()).array()*Vector3r(hSize.col(i)-refHSize.col(i)).array()).matrix();
			GLUtils::Parallelepiped(scaledHSize.col(0),scaledHSize.col(1),scaledHSize.col(2));
		} else 
		{
			GLUtils::Parallelepiped(hSize.col(0),hSize.col(1),hSize.col(2));
		}
	glPopMatrix();
}

void Renderer::resetSpecularEmission(){
	glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, 128);
	const GLfloat glutMatSpecular[4]={.6,.6,.6,1};
	const GLfloat glutMatEmit[4]={.1,.1,.1,.5};
	glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,glutMatSpecular);
	glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,glutMatEmit);
}


void Renderer::setLighting(){
	// recompute emissive light colors for highlighted bodies
	Real now=TimingInfo::getNow(/*even if timing is disabled*/true)*1e-9;
	highlightEmission0[0]=highlightEmission0[1]=highlightEmission0[2]=.8*normSquare(now,1);
	highlightEmission1[0]=highlightEmission1[1]=highlightEmission0[2]=.5*normSaw(now,2);

	glClearColor(bgColor[0],bgColor[1],bgColor[2],1.0);

	// set light sources
	glLightModelf(GL_LIGHT_MODEL_TWO_SIDE,1); // important: do lighting calculations on both sides of polygons

	const GLfloat pos[4]	= {(GLfloat)lightPos[0],(GLfloat)lightPos[1],(GLfloat)lightPos[2],(GLfloat)1.0};
	const GLfloat ambientColor[4]={0.5,0.5,0.5,1.0};
	const GLfloat specularColor[4]={1,1,1,1.};
	const GLfloat diffuseLight[4] = { (GLfloat)lightColor[0], (GLfloat)lightColor[1], (GLfloat)lightColor[2], 1.0f };
	glLightfv(GL_LIGHT0, GL_POSITION,pos);
	glLightfv(GL_LIGHT0, GL_SPECULAR, specularColor);
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambientColor);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
	if (light1) glEnable(GL_LIGHT0); else glDisable(GL_LIGHT0);

	const GLfloat pos2[4]	= {(GLfloat)light2Pos[0],(GLfloat)light2Pos[1],(GLfloat)light2Pos[2],1.0};
	const GLfloat ambientColor2[4]={0.0,0.0,0.0,1.0};
	const GLfloat specularColor2[4]={.8,.8,.8,1.};
	const GLfloat diffuseLight2[4] = { (GLfloat)light2Color[0], (GLfloat)light2Color[1], (GLfloat)light2Color[2], 1.0f };
	glLightfv(GL_LIGHT1, GL_POSITION,pos2);
	glLightfv(GL_LIGHT1, GL_SPECULAR, specularColor2);
	glLightfv(GL_LIGHT1, GL_AMBIENT, ambientColor2);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuseLight2);
	if (light2) glEnable(GL_LIGHT1); else glDisable(GL_LIGHT1);

	glEnable(GL_LIGHTING);

	// show both sides of triangles
	glDisable(GL_CULL_FACE);

	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,1);
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,1);

	// http://www.sjbaker.org/steve/omniv/opengl_lighting.html
	glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);
	//Shared material settings
	resetSpecularEmission();


	// not sctrictly lighting related
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	//glEnable(GL_POLYGON_SMOOTH);
	glShadeModel(GL_SMOOTH);
	#ifdef __MINGW64__ 
		// see http://www.opengl.org/discussion_boards/showthread.php/133406-GL_RESCALE_NORMAL-in-VC
		// why is glext.h not in mingw:
		//    * http://sourceforge.net/projects/mingw-w64/forums/forum/723797/topic/3572810
		glEnable(GL_NORMALIZE);
	#else
		glEnable(GL_RESCALE_NORMAL);
	#endif
	// important for rendering text, to avoid repetivie glDisable(GL_TEXTURE_2D) ... glEnable(GL_TEXTURE_2D)
	// glBindTexture(GL_TEXTURE_2D,0);
};



void Renderer::pyHandleCustomCtorArgs(py::tuple& args, py::dict& kw){
	if(py::len(kw)>0){
		string keys; py::list k(kw.keys());
		for(int i=0; i<py::len(k); i++) keys+=(i>0?", ":"")+py::extract<string>(k[i])();
		Master().instance().checkApi(10102,"Constructing Renderer with keywords ("+keys+") will have no effect unless passed to GlSetup/S.gl.",/*pyWarn*/true);
	}
}


void Renderer::render(const shared_ptr<Scene>& _scene, bool _withNames, bool _fastDraw){
	if(!initDone || _scene->glDirty || (_scene->gl && _scene->gl->dirty)){
		init(_scene);
		_scene->glDirty=false;
		if(_scene->gl) _scene->gl->dirty=false;
	}
	assert(initDone);

	switch(fast){
		case FAST_ALWAYS: fastDraw=true; break;
		case FAST_UNFOCUSED: fastDraw=_fastDraw; break;
		case FAST_NEVER: fastDraw=false; break;
	}

	// make a copy to see if it were true the whole time
	bool wasSetRefNow(setRefNow);

	withNames=_withNames; // used in many methods
	if(withNames) glNamedObjects.clear();

	// acquire shared_ptr to scene -- to be released at the end of this function
	scene=_scene;
	// acquire the weak_ptr, which will not be released until next assignment, but must check before every access
	weakScene=scene;
	

	// smuggle scene and ourselves into GLViewInfo for use with GlRep and field functors
	viewInfo.scene=scene.get();
	viewInfo.renderer=this;

	setLighting();
	drawPeriodicCell();

	fieldDispatcher.scene=scene.get(); fieldDispatcher.updateScenePtr();

	for(auto& f: scene->fields){
		fieldDispatcher(f,&viewInfo);
	}

	if(engines){
		for(const auto& e: scene->engines){
			if(!e || e->dead) continue; // !e should not happen, but make sure
			glScopedName name(viewInfo,e,shared_ptr<Node>());
			e->render(viewInfo);
		}
	}

	for(const shared_ptr<GlExtraDrawer>& d: extraDrawers){
		if(!d || d->dead) continue;
		glPushMatrix();
			d->render(viewInfo);
		glPopMatrix();
	}
	
	// if ref positions were set the whole time, unset here, it is done for all nodes
	if(setRefNow && wasSetRefNow){ setRefNow=false; }

	if(withNames) cerr<<"render(withNames==true) done, "<<glNamedObjects.size()<<" objects inserted"<<endl;

	// release the shared_ptr; must be GIL-protected since descruction of Python-constructed object without GIL causes crash
	{ GilLock lock; scene.reset(); }
}

void Renderer::setLightHighlighted(int highLev){
	// highLev can be <=0 (for 0), or 1 (for 1)
	const Vector3r& h=(highLev<=0?highlightEmission0:highlightEmission1);
	glMaterialv(GL_FRONT_AND_BACK,GL_EMISSION,h);
	glMaterialv(GL_FRONT_AND_BACK,GL_SPECULAR,h);
}

void Renderer::setLightUnhighlighted(){ resetSpecularEmission(); }	

void Renderer::renderRawNode(shared_ptr<Node> node){
	Vector3r x;
	if(node->hasData<GlData>()){
		x=node->pos+node->getData<GlData>().dGlPos; // pos+dGlPos is already canonicalized, if periodic
		if(isnan(x[0])) return;
	}
	else{ x=(scene->isPeriodic?scene->cell->canonicalizePt(node->pos):node->pos); }
	if(likely(!fastDraw)){
		Quaternionr ori=(node->hasData<GlData>()?node->getData<GlData>().dGlOri:Quaternionr::Identity())*node->ori;
		glPushMatrix();
			GLUtils::setLocalCoords(x,ori);
			nodeDispatcher(node,viewInfo);
		glPopMatrix();
	} else {
		// don't even call the functor with fastDraw, that slows down
		glBegin(GL_POINTS); glVertex3v(x); glEnd();
	}
	// if(node->rep){ node->rep->render(node,&viewInfo); }
}


void Renderer::renderLogo(int wd, int ht){
	if(logoWd<=0) return;
	Vector2r offset(logoPos[0]>=0?logoPos[0]:wd+logoPos[0],logoPos[1]>=0?logoPos[1]:ht+logoPos[1]);
	const auto& data=getGlWooLogo();
	glLineWidth(logoWd);
	//glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor3v(logoColor);
	for(const auto& line: data){
		glBegin(GL_LINE_STRIP);
		for(const Vector2r& pt: line){
			glVertex2d(offset[0]+pt[0]*logoSize,offset[1]+pt[1]*logoSize);
		}
		glEnd();
	}
	glDisable(GL_LINE_SMOOTH);
	//glDisable(GL_BLEND);
};

#endif /* WOO_OPENGL */
