// © 2004 Olivier Galizzi <olivier.galizzi@imag.fr>
// © 2008 Václav Šmilauer <eudoxos@arcig.cz>
#pragma once
#ifdef WOO_OPENGL

#include"../supp/multimethods/DynLibDispatcher.hpp"
#include"../supp/pyutil/converters.hpp"

#include"../core/Dispatcher.hpp"
#include"../supp/opengl/OpenGLWrapper.hpp"

#include"Functors.hpp"
#include"NodeGlRep.hpp"

#include"GlData.hpp"

struct GlExtraDrawer: public Object{
	virtual void render(const GLViewInfo&);
	#define woo_gl_GlExtraDrawer__CLASS_BASE_DOC_ATTRS_PY \
		GlExtraDrawer,Object,"Performing arbitrary OpenGL drawing commands; called from :obj:`Renderer` (see :obj:`Renderer.extraDrawers`) once regular rendering routines will have finished.\n\nThis class itself does not render anything, derived classes should override the *render* method.", \
		((bool,dead,false,,"Deactivate the object (on error/exception).")) \
		,/*py*/ ; woo::converters_cxxVector_pyList_2way<shared_ptr<GlExtraDrawer>>(mod);
	WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_gl_GlExtraDrawer__CLASS_BASE_DOC_ATTRS_PY);
};
WOO_REGISTER_OBJECT(GlExtraDrawer);

struct GlExtra_EnergyTrackerGrid: public GlExtraDrawer{
	void render(const GLViewInfo&) override;
	#define woo_gl_GlExtra_EnergyTrackerGrid__CLASS_BASE_DOC_ATTRS \
		GlExtra_EnergyTrackerGrid,GlExtraDrawer,"Draw :obj:`S.energy.grid <woo.core.Scene.EnergyTrackerGrid>`, if used.", \
		((Vector3r,color,Vector3r(1,.5,.5),AttrTrait<>().rgbColor(),"Color to render the box."))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_gl_GlExtra_EnergyTrackerGrid__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(GlExtra_EnergyTrackerGrid);



struct Renderer: public Object{
	public:
		static const int numClipPlanes=3;

		bool pointClipped(const Vector3r& p);
		vector<Vector3r> clipPlaneNormals;
		//bool initDone;
		Vector3r viewDirection; // updated from GLViewer regularly
		GLViewInfo viewInfo; // update from GLView regularly
		static Vector3r highlightEmission0;
		static Vector3r highlightEmission1;

		// normalized saw signal with given periodicity, with values ∈ 〈0,1〉 */
		static Real normSaw(Real t, Real period){ Real xi=(t-period*((int)(t/period)))/period; /* normalized value, (0-1〉 */ return (xi<.5?2*xi:2-2*xi); }
		static Real normSquare(Real t, Real period){ Real xi=(t-period*((int)(t/period)))/period; /* normalized value, (0-1〉 */ return (xi<.5?0:1); }

		void drawPeriodicCell();
		
		// check API -- and nothing else
		void pyHandleCustomCtorArgs(py::args_& args, py::kwargs& kw) override;

	private:
		void resetSpecularEmission();
		void setLighting();

	public:
		GlFieldDispatcher fieldDispatcher;
		GlNodeDispatcher nodeDispatcher;
	WOO_DECL_LOGGER;
	public:
		// updated after every call to render, held only while rendering (!!)
		shared_ptr<Scene> scene;
		// semi-safe storage of last-rendered scene, used when scene is needed outside of the
		// rendering itself -- e.g. when doing selection, scene object is needed
		weak_ptr<Scene> weakScene;

		// GL selection mangement
		// in selection mode, put rendered obejcts one after another into an array
		// which can be then used to look that object up

		// static so that glScopedName can access it
		// selection-related things
		bool withNames;
		bool fastDraw;
		vector<shared_ptr<Object>> glNamedObjects;
		vector<shared_ptr<Node>> glNamedNodes;

		// passing >=0 to highLev causes the object the to be highlighted, regardless of whether it is selected or not
		struct glScopedName{
			bool highlighted;
			glScopedName(const GLViewInfo& viewInfo, const shared_ptr<Object>& s, const shared_ptr<Node>& n, int highLev=-1): highlighted(false){ init(viewInfo,s,n, highLev); }
			glScopedName(const GLViewInfo& viewInfo, const shared_ptr<Node>& n, int highLev=-1): highlighted(false){ init(viewInfo, n,n, highLev); }
			void init(const GLViewInfo& viewInfo, const shared_ptr<Object>& s, const shared_ptr<Node>& n, int highLev){
				const auto& renderer=viewInfo.renderer;
				if(!renderer->withNames){
					if(highLev>=0 || s.get()==renderer->selObj.get()){ renderer->setLightHighlighted(highLev); highlighted=true; }
					else { renderer->setLightUnhighlighted(); highlighted=false; }
				} else {
					renderer->glNamedObjects.push_back(s);
					renderer->glNamedNodes.push_back(n);
					::glPushName(renderer->glNamedObjects.size()-1);
				}
			};
			~glScopedName(){ glPopName(); }
		};

		void setLightHighlighted(int highLev=0);
		void setLightUnhighlighted();

		void init(const shared_ptr<Scene>&);
		void render(const shared_ptr<Scene>& scene, bool withNames, bool fastDraw);

		void setNodeGlData(const shared_ptr<Node>& n);

		void renderRawNode(shared_ptr<Node>);

		// OpenGL commands which render the logo, in 2d
		// coordinate system must be set by the caller!
		void renderLogo(int ht, int wd);

		const Vector3r& axisColor(short ax);

	enum{TIME_NONE=0,TIME_VIRT=1,TIME_REAL=2,TIME_STEP=4};
	enum{TIME_ALL=TIME_VIRT|TIME_REAL|TIME_STEP};
	enum{FAST_ALWAYS=0,FAST_UNFOCUSED,FAST_NEVER};

	void postLoad(Renderer&,void* attr);

	#define woo_gl_Renderer__CLASS_BASE_DOC_ATTRS_CTOR_PY \
		Renderer,Object,"Class responsible for rendering scene on OpenGL devices.", \
		((bool,initDone,false,AttrTrait<Attr::hidden|Attr::noSave>(),"Track initialization (don't save, since this initialized GLUT as well, which is needed at every run again).")) \
		/* GENERAL */ \
		((bool,engines,true,AttrTrait<>().startGroup("General"),"Call engine's rendering functions (if defined)")) \
		((bool,ranges,true,,"Show color scales for :obj:`Scene.ranges`")) \
		((bool,cell,true,,"Render periodic cell boundaries (periodic simulations only)")) \
		((bool,ghosts,false,,"Render objects crossing periodic cell edges by cloning them in multiple places (periodic simulations only).")) \
		((vector<shared_ptr<GlExtraDrawer>>,extraDrawers,,,"Additional rendering components (:obj:`GlExtraDrawer`).")) \
		((Vector3r,iniUp,Vector3r(0,0,1),,"Up vector of new views")) \
		((Vector3r,iniViewDir,Vector3r(-1,0,0),,"View direction of new views")) \
		((string,snapFmt,"/tmp/{id}.{#}.png",AttrTrait<>().filename(),"Format for saving snapshots; `{tag}` sequences are expanded with Scene.tags; a special `{#}` tag is expanded with snapshot number (so that older ones are not overwritten), starting from 0 and zero-padded to 4 decimal palces. File format is auto-detected from extension. Supported formats are .png, .jpg, .pdf, .svg, xfig, ps, eps.")) \
		((int,fast,true,AttrTrait<Attr::namedEnum>().namedEnum({{FAST_ALWAYS,{"always"}},{FAST_UNFOCUSED,{"unfocused"}},{FAST_NEVER,{"never"}}}),"When to use fast rendering; unfocused means when manipulating camera or the 3d windows is not focused, and framerate drops below *maxFps*.")) \
		/* SCALING */ \
		((bool,scaleOn,false,AttrTrait<>().startGroup("Scaling").buttons({"Reference now","setRefNow=True","use current positions and orientations as reference for scaling displacement/rotation."},/*showBefore*/false),"Whether :obj:`dispScale` and :obj:`rotScale` have any effect or not.")) \
		((bool,setRefNow,false,AttrTrait<>().noGui(),"Update reference positions/orientations the next time we run.")) \
		((Vector3r,dispScale,Vector3r(10,10,10),AttrTrait<Attr::triggerPostLoad>(),"Artificially enlarge (scale) displacements from nodes' :obj:`reference positions <GlData.refPos>` by this relative amount, so that they become better visible (independently in 3 dimensions); not enabled unless :obj:`scaleOn` is ``True``. When set to something else than ``(1,1,1)``, :obj:`scaleOn` is set to ``True`` automatically.")) \
		((Real,rotScale,((void)"disable scaling",1.),AttrTrait<Attr::triggerPostLoad>(),"Artificially enlarge (scale) rotations of bodies relative to their :ref:`reference orientation <GlData.refOri>`, so the they are better visible; no effect if 1.0 and unless :obj:`scaleOn` is set. If set to anything else than 1.0, :obj:`scaleOn` is set automatically.")) \
		((Real,zClipCoeff,4.,AttrTrait<>().range(Vector2r(sqrt(3.),10)),"Z-clipping coefficient, relative to scene radius (see http://www.libqglviewer.com/refManual/classqglviewer_1_1Camera.html#acd07c1b9464b935ad21bb38b7c27afca for details)")) \
		/* COLORS AND LIGHTING */ \
		((Vector3r,bgColor,Vector3r(.2,.2,.2),AttrTrait<>().rgbColor().startGroup("Colors and lighting"),"Color of the background canvas (RGB)")) \
		((bool,light1,true,,"Turn light 1 on.")) \
		((Vector3r,lightPos,Vector3r(75,130,0),,"Position of OpenGL light source in the scene.")) \
		((Vector3r,lightColor,Vector3r(0.6,0.6,0.6),AttrTrait<>().rgbColor(),"Per-color intensity of primary light (RGB).")) \
		((bool,light2,true,,"Turn light 2 on.")) \
		((Vector3r,light2Pos,Vector3r(-130,75,30),,"Position of secondary OpenGL light source in the scene.")) \
		((Vector3r,light2Color,Vector3r(0.5,0.5,0.1),AttrTrait<>().rgbColor(),"Per-color intensity of secondary light (RGB).")) \
		((int,showTime,(TIME_VIRT|TIME_STEP),AttrTrait<>().bits({"virt","real","step"}),"Control whether virtual time, real time and step number are displayed in the 3d view.")) \
		((bool,showDate,false,,"Show human date and clock time in the 3d view.")) \
		((Vector3r,dateColor,Vector3r(.6,.6,.6),AttrTrait<>().rgbColor(),"Date color")) \
		((Vector3r,virtColor,Vector3r(1.,1.,1.),AttrTrait<>().rgbColor(),"Virtual time color")) \
		((Vector3r,realColor,Vector3r(0,.5,.5),AttrTrait<>().rgbColor(),"Real time color")) \
		((Vector3r,stepColor,Vector3r(0,.5,.5),AttrTrait<>().rgbColor(),"Step number color")) \
		((int,grid,0,AttrTrait<>().bits({"yz","zx","xy"}),"Show axes planes with grid")) \
		((bool,oriAxes,true,,"Show orientation axes in the 3d view (in the upper left corner)")) \
		((int,oriAxesPx,50,AttrTrait<>().range(Vector2i(10,100)),"Maximum pixel size of orientation axes in the corner.")) \
		((Vector3r,colorX,Vector3r(1,.1,0),AttrTrait<>().rgbColor(),"X-axis color")) \
		((Vector3r,colorY,Vector3r(1,1,0),AttrTrait<>().rgbColor(),"Y-axis color")) \
		((Vector3r,colorZ,Vector3r(.1,1,0),AttrTrait<>().rgbColor(),"Z-axis color")) \
		((int,logoSize,50,,"Size of the bigger size of the logo, in pixels")) \
		((Vector2i,logoPos,Vector2i(-64,-60),,"Position of the logo; negative values count from the other side of the window.")) \
		((Vector3r,logoColor,Vector3r(1.,1.,1.),AttrTrait<>().rgbColor(),"Logo color")) \
		((Real,logoWd,1.8,AttrTrait<>().range(Vector2r(0,10)),"Width of the logo stroke; set to non-positive value to disable the logo.")) \
		/* SELECTION AND CLIPPING */ \
		((vector<shared_ptr<Node>>,clipPlanes,,AttrTrait<Attr::triggerPostLoad>().startGroup("Selection & Clipping"),"Clipping plane definitions (local :math:`x`-axis defines the clipping plane; activity of the plane is determined by whether :obj:`woo.core.Node.rep` is something (active; otherwise unused object) or ``None``.)")) \
		((shared_ptr<Object>,selObj,,,"Object which was selected by the user (access only via woo.qt.selObj).")) \
		((shared_ptr<Node>,selObjNode,,AttrTrait<Attr::readonly>(),"Node associated to the selected object (recenters scene on that object upon selection)")) \
		((string,selFunc,"import woo.qt\nwoo.qt.onSelection",,"Python expression to be called (by textually appending '(woo.gl.Renderer.selOBj)' or '(None)') at object selection/deselection. If empty, no function will be called. Any imports must be explicitly mentioned in the string.")) \
		((bool,selFollow,false,,"Keep the scene centered at :obj:`selObjNode`.")) \
		/* PERFORMANCE */ \
		((int,maxFps,10,AttrTrait<>().startGroup("Performance"),"Maximum frame rate for the OpenGL display")) \
		((Real,renderTime,NaN,AttrTrait<>().readonly().timeUnit(),"Time for rendering one frame (smoothed)")) \
		((Real,fastRenderTime,NaN,AttrTrait<>().readonly().timeUnit(),"Time for fast-rendering one frame (smoothed)")) \
		,/*ctor*/ postLoad(*this,NULL); /* initializes clipPlanes */ \
		,/*py*/ \
			.def_readonly("nodeDispatcher",&Renderer::nodeDispatcher) \
			; \
			_classObj.attr("timeVirt")=(int)Renderer::TIME_VIRT; \
			_classObj.attr("timeReal")=(int)Renderer::TIME_REAL; \
			_classObj.attr("timeStep")=(int)Renderer::TIME_STEP; 
	WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR_PY(woo_gl_Renderer__CLASS_BASE_DOC_ATTRS_CTOR_PY);
};
WOO_REGISTER_OBJECT(Renderer);

#endif /* WOO_OPENGL */
