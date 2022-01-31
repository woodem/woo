#ifdef WOO_OPENGL
// use local includes, since MOC generated files do that as well
#include"OpenGLManager.hpp"
#include"../core/Timing.hpp"

WOO_IMPL_LOGGER(OpenGLManager);

OpenGLManager* OpenGLManager::self=NULL;

OpenGLManager::OpenGLManager(QObject* parent): QObject(parent){
	if(self) throw runtime_error("OpenGLManager instance already exists, uses OpenGLManager::self to retrieve it.");
	self=this;
	// move the whole rendering thing to a different thread
	// this should make the UI responsive regardless of how long the rendering takes
	// BROKEN!!
	#if 0
		renderThread=new QThread(); // is the thread going to be deleted later automatically?!
		this->moveToThread(renderThread);
		renderThread->start();
	#endif
	// Renderer::init(); called automatically when Renderer::render() is called for the first time
	viewsMutexMissed=0;
	frameMeasureTime=0;
	frameSaveState=0;
	connect(this,SIGNAL(createView()),this,SLOT(createViewSlot()));
	connect(this,SIGNAL(resizeView(int,int,int)),this,SLOT(resizeViewSlot(int,int,int)));
	// important: deferred
	QObject::connect(this,&OpenGLManager::closeView,this,&OpenGLManager::closeViewSlot,Qt::QueuedConnection);
	connect(this,SIGNAL(startTimerSignal()),this,SLOT(startTimerSlot()),Qt::QueuedConnection);
}

void OpenGLManager::timerEvent(QTimerEvent* event){
#if 1
	if(views.empty() || !views[0]) return;
	std::unique_lock lock(viewsMutex,std::try_to_lock);
	if(!lock.owns_lock()) return;
	if(views.size()>1) LOG_WARN("Only one (primary) view will be rendered.");
	Real t;
	const auto& s=Master::instance().getScene();
	if(!s) return;
	shared_ptr<Renderer> renderer=s->ensureAndGetRenderer();
		
	if(s->glDirty && s->gl){
		if(s->gl->qglviewerState.empty()){ LOG_INFO("Not setting QGLViewer state (empty)"); }
		else { LOG_INFO("Setting QGLViewer state."); views[0]->setState(s->gl->qglviewerState); }
	}

	bool measure=(renderer && frameMeasureTime>=renderer->maxFps);
	if(measure){ t=woo::TimingInfo::getNow(/*evenIfDisabled*/true); }

	/* ------------------ */
	views[0]->updateGL();
	/* -------------------*/

	if(measure){
		frameMeasureTime=0;
		t=1e-9*(woo::TimingInfo::getNow(/*evenIfDisabled*/true)-t); // in seconds
		Real& dt(renderer->fastDraw?renderer->fastRenderTime:renderer->renderTime);
		if(isnan(dt)) dt=t;
		dt=.6*dt+.4*(t);
	} else {
		frameMeasureTime++;
	}

	// every 50 frames, save state
	if(frameSaveState<views[0]->framesDone+50){
		s->gl->qglviewerState=views[0]->getState();
	}

	if(renderer && maxFps!=renderer->maxFps){
		killTimer(renderTimerId);
		maxFps=renderer->maxFps;
		renderTimerId=startTimer(1000/renderer->maxFps);
	}
#else
	// this implementation makes the GL idle on subsequent timers, if the rednering took longer than one timer shot
	// as many tim ers as the waiting took are the idle
	// this should improve ipython-responsivity under heavy GL loads
	boost::mutex::scoped_try_lock lock(viewsMutex);
	if(lock.owns_lock()){
		if(viewsMutexMissed>1) viewsMutexMissed-=1;
		else{
			viewsMutexMissed=0;
			for(const auto& view: views){ if(view) view->updateGL(); }
		}
	} else {
		viewsMutexMissed+=1;
	}
#endif
}

void OpenGLManager::createViewSlot(){
	std::scoped_lock lock(viewsMutex);
	if(views.size()==0){
		views.push_back(make_shared<GLViewer>(0,/*shareWidget*/(QGLWidget*)0));
	} else {
		throw runtime_error("Secondary views not supported");
		//views.push_back(shared_ptr<GLViewer>(new GLViewer(views.size(),renderer,views[0].get())));
	}	
}

void OpenGLManager::resizeViewSlot(int id, int wd, int ht){
	views[id]->resize(wd,ht);
}

void OpenGLManager::closeViewSlot(int id){
	std::scoped_lock lock(viewsMutex);
	for(long i=(long)views.size()-1; (!views[i]) && i>=0; i--){ views.resize(i); } // delete empty views from the end
	if(id<0){ // close the last one existing
		assert(*views.rbegin()); // this should have been sanitized by the loop above
		views.resize(views.size()-1); // releases the pointer as well
	}
	if(id==0){
		LOG_DEBUG("Closing primary view.");
		if(views.size()==1) views.clear();
		else{ LOG_INFO("Cannot close primary view, secondary views still exist."); }
	}
}
void OpenGLManager::centerAllViews(){
	std::scoped_lock lock(viewsMutex);
	for(const shared_ptr<GLViewer>& g: views){ if(!g) continue; g->centerScene(); }
}
void OpenGLManager::startTimerSlot(){
	// get scene and renderer directly the first time
	const auto& s=Master::instance().getScene();
	if(!s) return; // should not happen
	const auto& rr=s->ensureAndGetRenderer();
	maxFps=(rr?rr->maxFps:15); // remember the last value
	renderTimerId=startTimer(1000/maxFps);
}

int OpenGLManager::waitForNewView(float timeout){
	size_t origViewCount=views.size();
	emitCreateView();
	float t=0;
	while(views.size()!=origViewCount+1){
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		t+=.05;
		// wait at most 5 secs
		if(t>=timeout) {
			LOG_ERROR("Timeout waiting for the new view to open, giving up."); return -1;
		}
	}
	return (*views.rbegin())->viewId; 
}

#endif /* WOO_OPENGL */
