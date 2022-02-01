#pragma once

#include"Cell.hpp"
#include"Engine.hpp"
#include"EnergyTracker.hpp"
#include"Plot.hpp"
#include"LabelMapper.hpp"
#include"Preprocessor.hpp"
#include"ScalarRange.hpp"

#ifdef WOO_OPENCL
	#define __CL_ENABLE_EXCEPTIONS
	#include<CL/cl.hpp>
#endif

#ifdef WOO_OPENGL
	#include"DisplayParameters.hpp"
	struct Renderer;
	// TODO? move GlSetup to core so that we keep modules neatly separated
	#include"../gl/GlSetup.hpp"
#endif

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255 
#endif

struct Bound;
struct Field;

struct Scene: public Object{
	private:
		// this is managed by methods of Scene exclusively
		std::mutex runMutex;
		bool runningFlag;
		std::thread::id bgThreadId;
		// this will be std::atomic<bool>
		// once libstdc++ headers are accepted by clang++
		bool stopFlag; 
		bool stopFlagSet(){ std::scoped_lock l(runMutex); return stopFlag; }
		shared_ptr<std::exception> except;
	public:
		// interface for python,
		void pyRun(long steps=-1, bool wait=false, Real time_=NaN);
		void pyStop();         
		void pyOne();         
		void pyWait(float timeout=0);         
		bool running(); 
		void backgroundLoop();

		// initialize tags (author, date, time)
		void fillDefaultTags();
		// advance by one iteration by running all engines
		void doOneStep();
		// force: run regardless of step number, otherwise only when selfTestEvery is favorable
		void selfTest_maybe(bool force=false);
		void pySelfTest(){ selfTest_maybe(/*force*/true); }

		void postLoad(Scene&,void*);
		void preSave(Scene&){ preSaveDuration=pyGetDuration(); }

		#ifdef WOO_OPENCL
			shared_ptr<cl::Platform> platform;
			shared_ptr<cl::Device> device;
			shared_ptr<cl::Context> context;
			shared_ptr<cl::CommandQueue> queue;
		#endif

		#ifdef WOO_OPENGL
			// implementation is in pkg/gl/Renderer(!!!)
			void ensureGl();
			shared_ptr<Renderer> ensureAndGetRenderer();
			shared_ptr<Object> pyEnsureAndGetRenderer(); // return cast to Object, since .def needs full type
			shared_ptr<GlSetup> pyGetGl(void);
			void pySetGl(const shared_ptr<GlSetup>&);
		#else
			shared_ptr<Object> ensureAndGetRenderer() { return shared_ptr<Object>(); }
		#endif

		// keep track of created labels; delete those which are no longer active and so on
		// std::set<std::string> pyLabels;

		std::vector<shared_ptr<Engine> > pyEnginesGet();
		void pyEnginesSet(const std::vector<shared_ptr<Engine> >&);
		shared_ptr<ScalarRange> getRange(const std::string& l) const;
		
		struct pyTagsProxy{
			Scene* scene;
			pyTagsProxy(Scene* _scene): scene(_scene){};
			std::string getItem(const std::string& key);
			void setItem(const std::string& key, const std::string& value);
			void delItem(const std::string& key);
			py::list keys();
			py::list items();
			py::list values();
			bool has_key(const std::string& key);
			void update(const pyTagsProxy& b);
			static void pyRegisterClass(py::module_& mod){
				py::class_<Scene::pyTagsProxy>(mod,"TagsProxy")
					.def("__getitem__",&pyTagsProxy::getItem)
					.def("__setitem__",&pyTagsProxy::setItem)
					.def("__delitem__",&pyTagsProxy::delItem)
					.def("has_key",&pyTagsProxy::has_key)
					.def("__contains__",&pyTagsProxy::has_key)
					.def("keys",&pyTagsProxy::keys)
					.def("update",&pyTagsProxy::update)
					.def("items",&pyTagsProxy::items)
					.def("values",&pyTagsProxy::values)
				;
			}
		};
		pyTagsProxy pyGetTags(){ return pyTagsProxy(this); }
		shared_ptr<Cell> pyGetCell(){ return (isPeriodic?cell:shared_ptr<Cell>()); }

		std::chrono::system_clock::time_point clock0;
		bool clock0adjusted; // flag to adjust clock0 in postLoad only once
		long pyGetDuration(){ return (long)std::chrono::duration<double>(std::chrono::system_clock::now()-clock0).count(); }

		typedef std::map<std::string,std::string> StrStrMap;

		void ensureCl(); // calls initCL or throws exception if compiled without OpenCL
		#ifdef WOO_OPENCL
			void initCl(); // initialize OpenCL using clDev
		#endif


		/*
			engineLoopMutex synchronizes access to particles/engines/contacts between
			* simulation itself (at the beginning of Scene::doOneStep) and
			* user access from python via Scene.paused() context manager
			PausedContextManager obtains the lock (warns every few seconds that the lock was not yet
			obtained, for diagnosis of deadlock) in __enter__ and releases it in __exit__.
		*/

		// workaround; will be removed once
		// http://stackoverflow.com/questions/12203769/boosttimed-mutex-guarantee-of-acquiring-the-lock-when-other-thread-unlocks-i
		// is solved
		#define WOO_LOOP_MUTEX_HELP
		#ifdef WOO_LOOP_MUTEX_HELP
			bool engineLoopMutexWaiting;
		#endif
		std::timed_mutex engineLoopMutex;
		struct PausedContextManager{
			shared_ptr<Scene> scene;
			bool allowBg;
			bool locked=true;
			#ifdef WOO_LOOP_MUTEX_HELP
				bool& engineLoopMutexWaiting;
			#endif
			// stores reference to mutex, but does not lock it yet
			PausedContextManager(const shared_ptr<Scene>& _scene, bool _allowBg): scene(_scene),allowBg(_allowBg)
				#ifdef WOO_LOOP_MUTEX_HELP
					, engineLoopMutexWaiting(scene->engineLoopMutexWaiting)
				#endif
			{}
			void __enter__();
			void __exit__(py::object exc_type, py::object exc_value, py::object traceback);
			static void pyRegisterClass(py::module_& mod){
				py::class_<PausedContextManager>(mod,"PausedContextManager")
					.def("__enter__",&PausedContextManager::__enter__)
					.def("__exit__",&PausedContextManager::__exit__)
				;
			}
		};
		PausedContextManager* pyPaused(bool allowBg=false){ return new PausedContextManager(static_pointer_cast<Scene>(shared_from_this()),allowBg); }

		// override Object::boostSave, to set lastSave correctly
		void boostSave(const string& out) override;
		void saveTmp(const string& slot, bool quiet=true);

		// expand {tagName} in given string
		string expandTags(const string& s) const;

		// constants for some values of subStep
		enum {SUBSTEP_INIT=-1,SUBSTEP_PROLOGUE=0};


		#ifdef WOO_OPENGL
			#define woo_core_Scene__ATTRS__OPENGL \
				((vector<shared_ptr<DisplayParameters>>,dispParams,,AttrTrait<>().noGui(),"Saved display states.")) \
				((shared_ptr<GlSetup>,gl,/* no default since the type is incomplete here ... really?! */,AttrTrait<Attr::hidden>(),"Settings related to rendering; default instance is created on-the-fly when requested from Python.")) \
				((bool,glDirty,true,AttrTrait<Attr::readonly>(),"Flag to re-initalize functors, colorscales and restore QGLViewer before rendering."))
			#define woo_core_Scene__PY__OPENGL .add_property_readonly("renderer",&Scene::pyEnsureAndGetRenderer) /* for retrieving from Python */ .add_property("gl",&Scene::pyGetGl,&Scene::pySetGl)
		#else
			#define woo_core_Scene__ATTRS__OPENGL
			#define woo_core_Scene__PY__OPENGL
		#endif

	#define woo_core_Scene__CLASS_BASE_DOC_ATTRS_INI_CTOR_DTOR_PY \
		Scene,Object,ClassTrait().doc("Object comprising the whole simulation.").section("Scene","TODO",{"SceneAttachedObject","Field","LabelMapper","Cell","EnergyTracker"}) \
		, /*attrs*/ \
		((Real,dt,NaN,AttrTrait<>().timeUnit(),"Current timestep for integration.")) \
		((Real,nextDt,NaN,AttrTrait<>().timeUnit(),"Timestep for the next step (if not NaN, :obj:`dt` is automatically replaced by this value at the end of the step).")) \
		((Real,dtSafety,.9,,"Safety factor for automatically-computed timestep.")) \
		((Real,throttle,0,,"Insert this delay before each step when running in loop; useful for slowing down simulations for visual inspection if they happen too fast.\n\n.. note:: Negative values will not make the simulation run faster.")) \
		((long,step,0,AttrTrait<Attr::readonly>(),"Current step number")) \
		((bool,subStepping,false,,"Whether we currently advance by one engine in every step (rather than by single run through all engines).")) \
		((int,subStep,-1,AttrTrait<Attr::readonly>(),"Number of sub-step; not to be changed directly. -1 means to run loop prologue (cell integration), 0…n-1 runs respective engines (n is number of engines), n runs epilogue (increment step number and time.")) \
		((Real,time,0,AttrTrait<Attr::readonly>().timeUnit(),"Simulation time (virtual time) [s]")) \
		((long,stopAtStep,0,,"Iteration after which to stop the simulation.")) \
		((Real,stopAtTime,NaN,,":obj:`time` around which to stop the simulation.\n\n.. note:: This value is not exact, has the granularity of :math:`\\Dt`: simulation will stopped at the moment when :obj:`stopAtTime` ≤ :obj:`time` < :obj:`dt` + :obj:`stopAtTime`. This condition may have some corner cases due to floating-point comparisons involved.")) \
		((string,stopAtHook,"",,"Python command given as string executed when :obj:`stopAtTime` or :obj:`stopAtStep` cause the simulation to be stopped.\n\n .. note:: This hook will *not* be run when a running simulation is paused manually, or through S.pause in the script. To trigger this hook from within a :obj:`PyRunner`, do something like ``S.stopAtStep=S.step`` which will activate it when the current step finishes.")) \
		\
		((bool,isPeriodic,false,/*exposed as "periodic" in python */AttrTrait<Attr::hidden>(),"Whether periodic boundary conditions are active.")) \
		((bool,trackEnergy,false,,"Whether energies are being tracked.")) \
		((bool,deterministic,false,,"Hint for engines to order (possibly at the expense of performance) arithmetic operations to be independent of thread scheduling; this results in simulation with the same initial conditions being always the same. This is disabled by default, because of performance issues. Note that deterministic result is not \"more correct\" (neither physically, nor theoretically) than other result with different operation ordering; it is only self-consistent and feels better.")) \
		((int,selfTestEvery,0,,"Periodicity with which consistency self-tests will be run; 0 to run only in the very first step, negative to disable.")) \
		\
		((Vector2i,clDev,Vector2i(-1,-1),AttrTrait<Attr::triggerPostLoad>(),"OpenCL device to be used; if (-1,-1) (default), no OpenCL device will be initialized until requested. Saved simulations should thus always use the same device when re-loaded.")) \
		((Vector2i,_clDev,Vector2i(-1,-1),AttrTrait<Attr::readonly|Attr::noSave>(),"OpenCL device which is really initialized (to detect whether clDev was changed manually to avoid spurious re-initializations from postLoad")) \
		\
		((AlignedBox3r,boxHint,AlignedBox3r(),,"Hint for displaying the scene; overrides node-based detection. Set an element to empty box to disable.")) \
		\
		((bool,runInternalConsistencyChecks,true,AttrTrait<Attr::hidden>(),"Run internal consistency check, right before the very first simulation step.")) \
		\
		((StrStrMap,tags,,AttrTrait<Attr::hidden>(),"Arbitrary key=value associations (tags like mp3 tags: author, date, version, description etc.)")) \
		((shared_ptr<LabelMapper>,labels,make_shared<LabelMapper>(),AttrTrait<>().noGui().readonly(),"Atrbitrary key=object, key=list of objects, key=py::object associations which survive saving/loading. Labeled objects are automatically added to this container. This object is more conveniently accessed through the :obj:`lab` attribute, which exposes the mapping as attributes of that object rather than as a dictionary.")) \
		\
		((string,uiBuild,"",,"Command to run when a new main-panel UI should be built for this scene (called when the Controller is opened with this simulation, or the simulation is new to the controller).")) \
		\
		\
		((vector<shared_ptr<Engine>>,engines,,,"Engines sequence in the simulation (direct access to the c++ sequence is shadowed by python property which access it indirectly).")) \
		((vector<shared_ptr<Engine>>,_nextEngines,,AttrTrait<Attr::hidden>(),"Engines to be used from the next step on; is returned transparently by S.engines if in the middle of the loop (controlled by subStep>=0).")) \
		((shared_ptr<EnergyTracker>,energy,make_shared<EnergyTracker>(),AttrTrait<Attr::readonly>().noGui(),"Energy values, if energy tracking is enabled.")) \
		((vector<shared_ptr<Field>>,fields,,AttrTrait<Attr::triggerPostLoad>().noGui(),"Defined simulation fields.")) \
		((shared_ptr<Cell>,cell,make_shared<Cell>(),AttrTrait<Attr::hidden>(),"Information on periodicity; only should be used if Scene::isPeriodic.")) \
		((std::string,lastSave,,AttrTrait<Attr::readonly>(),"Name under which the simulation was saved for the last time; used for reloading the simulation. Updated automatically, don't change.")) \
		((long,preSaveDuration,,AttrTrait<Attr::readonly>().noGui(),"Wall clock duration this Scene was alive before being saved last time; this count is incremented every time the scene is saved. When Scene is loaded, it is used to construct clock0 as current_local_time - lastSecDuration.")) \
		woo_core_Scene__ATTRS__OPENGL \
		((vector<shared_ptr<ScalarRange>>,ranges,,,"User-defined :obj:`~woo.core.ScalarRange` objects, to be rendered as colormaps.")) \
		((vector<shared_ptr<ScalarRange>>,autoRanges,,AttrTrait<Attr::readonly>(),":obj:`~woo.core.ScalarRange` colormaps automatically obtained from renderes and engines; displayed only when actually used.")) \
		((vector<shared_ptr<Object>>,any,,,"Storage for arbitrary Objects; meant for storing and loading static objects like Gl1_* functors to restore their parameters when scene is loaded.")) \
		((py::object,pre,,AttrTrait<>().noGui(),"Preprocessor used for generating this simulation; to be only used in user scripts to query preprocessing parameters, not in c++ code.")) \
		\
		/* postLoad checks the new value is not None */ \
		((shared_ptr<Plot>,plot,make_shared<Plot>(),AttrTrait<Attr::triggerPostLoad>().noGui(),"Data and settings for plots.")) \
		((shared_ptr<SceneCtrl>,ctrl,/*empty by default*/,AttrTrait<Attr::triggerPostLoad>(),"High-level control interface for this particular scene.")) \
		, /*ini*/ \
		, /*ctor*/ \
			fillDefaultTags(); \
			runningFlag=false; \
			clock0=std::chrono::system_clock::now(); \
			clock0adjusted=false; \
		, /* dtor */ pyStop();  \
		, /* py */ \
		woo_core_Scene__PY__OPENGL \
		.add_property_readonly("tags",&Scene::pyGetTags,"Arbitrary key=value associations (tags like mp3 tags: author, date, version, description etc.") \
		.add_property_readonly("duration",&Scene::pyGetDuration,"Number of (wall clock) seconds this instance is alive (including time before being loaded from file") \
		.add_property_readonly("cell",&Scene::pyGetCell,"Periodic space configuration (is None for aperiodic scene); set :obj:`periodic` to enable/disable periodicity") \
		.def_readwrite("periodic",&Scene::isPeriodic,"Set whether the scene is periodic or not") \
		.add_property("engines",&Scene::pyEnginesGet,&Scene::pyEnginesSet,"Engine sequence in the simulation") \
		.add_property_readonly("_currEngines",WOO_PY_EXPOSE_COPY(Scene,&Scene::engines),"Current engines, debugging only") \
		.add_property_readonly("_nextEngines",WOO_PY_EXPOSE_COPY(Scene,&Scene::_nextEngines),"Next engines, debugging only") \
		.def("getRange",&Scene::getRange,"Retrieve a *ScalarRange* object by its label") \
		/* WOO_OPENCL */ .def("ensureCl",&Scene::ensureCl,"[for debugging] Initialize the OpenCL subsystem (this is done by engines using OpenCL, but trying to do so in advance might catch errors earlier)") \
		.def("saveTmp",&Scene::saveTmp,WOO_PY_ARGS(py::arg("slot")="",py::arg("quiet")=false),"Save into a temporary slot inside Master (loadable with O.loadTmp)") \
		\
		.def("run",&Scene::pyRun,WOO_PY_ARGS(py::arg("steps")=-1,py::arg("wait")=false,py::arg("time")=NaN)) \
		.def("stop",&Scene::pyStop) \
		.def("one",&Scene::pyOne) \
		.def("wait",&Scene::pyWait,py::arg("timeout")=0) \
		.def("setLastSave",[](const shared_ptr<Scene>& self, const string& s){ self->lastSave=s; }) \
		.add_property_readonly("running",&Scene::running) \
		.def("paused",&Scene::pyPaused,WOO_PY_ARGS(py::arg("allowBg")=false),WOO_PY_RETURN__TAKE_OWNERSHIP,"Return paused context manager; when *allowBg* is True, the context manager is a no-op in the engine background thread and works normally when called from other threads).") \
		.def("selfTest",&Scene::pySelfTest,"Run self-tests (they are usually run automatically with, see :obj:`selfTestEvery`).") \
		.def("expandTags",&Scene::expandTags,"Expand :obj:`tags` written as ``{tagName}``, returns the expanded string.") \
		; /* define nested classes */ \
		Scene::pyTagsProxy::pyRegisterClass(mod); \
		Scene::PausedContextManager::pyRegisterClass(mod);
	
	WOO_DECL__CLASS_BASE_DOC_ATTRS_INI_CTOR_DTOR_PY(woo_core_Scene__CLASS_BASE_DOC_ATTRS_INI_CTOR_DTOR_PY);
	WOO_DECL_LOGGER;
};

WOO_REGISTER_OBJECT(Scene);
