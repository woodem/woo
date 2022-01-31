
#include<woo/pkg/dem/Tracer.hpp>

#ifdef WOO_OPENGL
	#include<woo/lib/opengl/OpenGLWrapper.hpp>
	#include<woo/lib/opengl/GLUtils.hpp>
	#include<woo/pkg/gl/Renderer.hpp> // for displacement scaling
#endif

#include<woo/pkg/dem/Sphere.hpp>
#include<woo/pkg/dem/Clump.hpp>
#include<woo/pkg/dem/Gl1_DemField.hpp> // for setOurSceneRanges

WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_TraceVisRep__CLASS_BASE_DOC_ATTRS_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_BoxTraceTimeSetter__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_Tracer__CLASS_BASE_DOC_ATTRS_PY);


WOO_PLUGIN(dem,(Tracer)(TraceVisRep)(BoxTraceTimeSetter));

void BoxTraceTimeSetter::run(){
	for(const auto& n: field->nodes){
		if(!n->rep || !n->rep->isA<TraceVisRep>()) continue;
		if(!box.contains(node?node->glob2loc(n->pos):n->pos)) continue;
		n->rep->cast<TraceVisRep>().t0=scene->time;
	}
}

#ifdef WOO_OPENGL
void BoxTraceTimeSetter::render(const GLViewInfo&){
	if(isnan(glColor)) return;
	if(!node) GLUtils::AlignedBox(box,CompUtils::mapColor(glColor));
	else {
		glPushMatrix();
			GLUtils::setLocalCoords(node->pos,node->ori);
			GLUtils::AlignedBox(box,CompUtils::mapColor(glColor));
		glPopMatrix();
	}
}
#endif



WOO_IMPL_LOGGER(TraceVisRep);

void TraceVisRep::compress(int ratio){
	assert(tracer);
	int i;
	int skip=(tracer->compSkip<0?ratio:tracer->compSkip);
	for(i=0; ratio*i+skip<(int)pts.size(); i++){
		pts[i]=pts[ratio*i+skip];
		scalars[i]=scalars[ratio*i+skip];
	}
	writeIx=i;
}
void TraceVisRep::addPoint(const Vector3r& p, const Real& scalar){
	assert(tracer);
	// with mindist, maybe don't write anything at all
	if(flags&FLAG_MINDIST){
		size_t lastIx=(writeIx>0?writeIx-1:pts.size());
		if((p-pts[lastIx]).norm()<tracer->minDist) return;
	}
	// write data
	pts[writeIx]=p;
	scalars[writeIx]=scalar;
	if(!times.empty()) times[writeIx]=tracer->scene->time;
	// compute next write index
	if(flags&FLAG_COMPRESS){
		if(writeIx>=pts.size()-1) compress(tracer->compress);
		else writeIx+=1;
	} else {
		writeIx=(writeIx+1)%pts.size();
	}
}


vector<Vector3r> TraceVisRep::pyPts_get() const{
	size_t i=0; vector<Vector3r> ret; ret.reserve(countPointData());
	Vector3r pt; Real scalar; Real time;
	while(getPointData(i++,pt,time,scalar)) ret.push_back(pt);
	return ret;
}

vector<Real> TraceVisRep::pyScalars_get() const{
	size_t i=0; vector<Real> ret; ret.reserve(countPointData());
	Vector3r pt; Real scalar; Real time;
	while(getPointData(i++,pt,time,scalar)) ret.push_back(scalar);
	return ret;
}

vector<Real> TraceVisRep::pyTimes_get() const{
	size_t i=0; vector<Real> ret; ret.reserve(countPointData());
	Vector3r pt; Real scalar; Real time;
	while(getPointData(i++,pt,time,scalar)) ret.push_back(time);
	return ret;
}

int TraceVisRep::pyIndexConvert(int i, const string& name) const {
	int cnt=(int)countPointData();
	if(i>=cnt || i<-cnt) woo::IndexError("TraceVisRep."+name+"("+to_string(i)+"): index out of range "+to_string(-cnt)+".."+to_string(cnt-1)+".");
	if(i>=0) return i;
	return cnt+i; // i<0
}

Real TraceVisRep::pyScalar_get(int i) const {
	Vector3r pt; Real scalar; Real time;
	bool ok=getPointData(pyIndexConvert(i,"scalar"),pt,time,scalar);
	if(!ok) throw std::logic_error("TraceVisRep::pyScalar_get: getPointData error for i="+to_string(i)+"?");
	return scalar;
}

Vector3r TraceVisRep::pyPt_get(int i) const {
	Vector3r pt; Real scalar; Real time;
	bool ok=getPointData(pyIndexConvert(i,"point"),pt,time,scalar);
	if(!ok) throw std::logic_error("TraceVisRep::pyPt_get: getPointData error for i="+to_string(i)+"?");
	return pt;
}

Real TraceVisRep::pyTime_get(int i) const {
	Vector3r pt; Real scalar; Real time;
	bool ok=getPointData(pyIndexConvert(i,"scalar"),pt,time,scalar);
	if(!ok) throw std::logic_error("TraceVisRep::pyTime_get: getPointData error for i="+to_string(i)+"?");
	return time;
}

size_t TraceVisRep::countPointData() const {
	if(flags&FLAG_COMPRESS) return writeIx-1;
	return pts.size();
}

bool TraceVisRep::getPointData(size_t i, Vector3r& pt, Real& time, Real& scalar) const {
	size_t ix;
	if(flags&FLAG_COMPRESS){
		ix=i;
		if(ix>=writeIx) return false;
	} else { // cycles through the array
		if(i>=pts.size()) return false;
		ix=(writeIx+i)%pts.size();
	}
	pt=pts[ix];
	scalar=scalars[ix];
	time=(times.empty()?NaN:times[ix]);
	return true;
}

#ifdef WOO_OPENGL
void TraceVisRep::render(const shared_ptr<Node>& n, const GLViewInfo* glInfo){
	if(isHidden() || !tracer) return;
	if(!tracer->glSmooth) glDisable(GL_LINE_SMOOTH);
	else glEnable(GL_LINE_SMOOTH);
	glDisable(GL_LIGHTING);
	bool scale=(glInfo->renderer->dispScale!=Vector3r::Ones() && glInfo->renderer->scaleOn && n->hasData<GlData>());
	glLineWidth(tracer->glWidth);
	const bool periodic=glInfo->scene->isPeriodic;
	Vector3i prevPeriod=Vector3i::Zero(); // silence gcc warning maybe-uninitialized
	int nSeg=0; // number of connected vertices (used when trace is interruped by NaN)
	glBegin(GL_LINE_STRIP);
		for(size_t i=0; i<pts.size(); i++){
			size_t ix;
			// FIXME: use getPointData here (copied code)
			// compressed start from the very beginning, till they reach the write position
			if(flags&FLAG_COMPRESS){
				ix=i;
				if(ix>=writeIx) break;
			// cycle through the array
			} else {
				ix=(writeIx+i)%pts.size();
			}
			if(!isnan(pts[ix][0])){
				Vector3r color;
				if(isnan(scalars[ix])){
					// if there is no scalar and no scalar should be saved, color by history position
					if(tracer->scalar==Tracer::SCALAR_NONE) color=tracer->lineColor->color((flags&FLAG_COMPRESS ? i*1./writeIx : i*1./pts.size()));
					// if other scalars are saved, use noneColor to not destroy tracer->lineColor range by auto-adjusting to bogus
					else color=tracer->noneColor;
				}
				else color=tracer->lineColor->color(scalars[ix]);
				if(isnan(color.maxCoeff())){
					if(nSeg>0){ glEnd(); glBegin(GL_LINE_STRIP); nSeg=0; } // break line, if there was something already
					continue; // point skipped completely
				}
				glColor3v(color);
				Vector3r pt(pts[ix]);
				if(periodic){
					// canonicalize the point, store the period
					Vector3i currPeriod;
					pt=glInfo->scene->cell->canonicalizePt(pt,currPeriod);
					// if the period changes between these two points, split the line (and don't render the segment in-between for simplicity)
					if(i>0 && currPeriod!=prevPeriod){
						if(nSeg>0){ glEnd(); glBegin(GL_LINE_STRIP); nSeg=0; } // only if there was sth already
						// point not skipped, only line interruped, so keep going
					}
					prevPeriod=currPeriod;
				}
				if(!scale) glVertex3v(pt);
				else{
					const auto& gl=n->getData<GlData>();
					// don't scale if refpos is invalid
					if(isnan(gl.refPos.maxCoeff())) glVertex3v(pt); 
					// x+(s-1)*(x-x0)
					else glVertex3v((pt+((glInfo->renderer->dispScale-Vector3r::Ones()).array()*(pt-gl.refPos).array()).matrix()).eval());
				}
				nSeg++;
			}
		}
	glEnd();
}
#endif

void TraceVisRep::resize(size_t size, bool saveTime){
	pts.resize(size,Vector3r(NaN,NaN,NaN));
	scalars.resize(size,NaN);
	if(saveTime) times.resize(size,NaN);
	else times.clear();
}

void TraceVisRep::consolidate(){
	LOG_WARN("This function is deprecated and no-op. TraceVisRep.pts is always returned in the proper order, as a copy of the internal circular buffer.");
}

void Tracer::resetNodesRep(bool setupEmpty, bool includeDead){
	auto& dem=field->cast<DemField>();
	std::scoped_lock lock(dem.nodesMutex);
	for(const auto& n: dem.nodes){
			/*
				FIXME: there is some bug in boost::python's shared_ptr allocator, so if a node has GlRep
				assigned from Python, we might crash here -- like this:

					#4  <signal handler called>
					#5  0x00000000004845d8 in ?? ()
					#6  0x00007fac585be422 in boost::python::converter::shared_ptr_deleter::operator()(void const*) () from /usr/lib/x86_64-linux-gnu/libboost_python-py27.so.1.53.0
					#7  0x00007fac568cc41e in boost::detail::sp_counted_base::release (this=0x5d6d940) at /usr/include/boost/smart_ptr/detail/sp_counted_base_gcc_x86.hpp:146
					#8  0x00007fac56aaed7b in ~shared_count (this=<optimized out>, __in_chrg=<optimized out>) at /usr/include/boost/smart_ptr/detail/shared_count.hpp:371
					#9  ~shared_ptr (this=<optimized out>, __in_chrg=<optimized out>) at /usr/include/boost/smart_ptr/shared_ptr.hpp:328
					#10 operator=<TraceVisRep> (r=<unknown type in /usr/local/lib/python2.7/dist-packages/woo/_cxxInternal_mt_debug.so, CU 0x13c9b4f, DIE 0x1cc8cb9>, this=<optimized out>) at /usr/include/boost/smart_ptr/shared_ptr.hpp:601
					#11 Tracer::resetNodesRep (this=this@entry=0x5066d20, setupEmpty=setupEmpty@entry=true, includeDead=includeDead@entry=false) at /home/eudoxos/woo/pkg/dem/Tracer.cpp:119

				As workaround, set the tracerSkip flag on the node and the tracer will leave it alone.

			
				CAUSE: see https://svn.boost.org/trac/boost/ticket/8290 and pkg/dem/ParticleContainer.cpp
			*/
			if(n->getData<DemData>().isTracerSkip()) continue;
			GilLock lock; // get GIL to avoid crash when destroying python-instantiated object
			if(setupEmpty){
				n->rep=make_shared<TraceVisRep>();
				auto& tr=n->rep->cast<TraceVisRep>();
				tr.resize(num,saveTime);
				tr.flags=(compress>0?TraceVisRep::FLAG_COMPRESS:0) | (minDist>0?TraceVisRep::FLAG_MINDIST:0);
				tr.t0=scene->time;
			} else {
				n->rep.reset();
			}
		}
	if(includeDead){
		for(const auto& n: dem.deadNodes){
			if(n->getData<DemData>().isTracerSkip()) continue;
			GilLock lock; // get GIL to avoid crash, see above
			n->rep.reset();
		}
	}
}

void Tracer::postLoad(Tracer&, void* attr){
	if(attr==&scalar || attr==&matStateIx || attr==NULL) nextReset=true;
}


void Tracer::run(){
	if(nextReset){
		resetNodesRep(/*setup empty*/true,/*includeDead*/false);
		lineColor->reset();
		nextReset=false;
	}
	switch(scalar){
		case SCALAR_NONE: lineColor->label="[index]"; break;
		case SCALAR_TRACETIME: lineColor->label="trace time"; break;
		case SCALAR_TIME: lineColor->label="time"; break;
		case SCALAR_VEL: lineColor->label="vel"; break;
		case SCALAR_ANGVEL: lineColor->label="angVel"; break;
		case SCALAR_SIGNED_ACCEL: lineColor->label="signed |accel|"; break;
		case SCALAR_RADIUS: lineColor->label="radius"; break;
		case SCALAR_NUMCON: lineColor->label="num. contacts"; break;
		case SCALAR_SHAPE_COLOR: lineColor->label="Shape.color"; break;
		case SCALAR_ORDINAL: lineColor->label="ordinal"+(ordinalMod>1?string(" % "+to_string(ordinalMod)):string());
		case SCALAR_KINETIC: lineColor->label="kinetic energy"; break;
		case SCALAR_MATSTATE: /*handled below*/; break;
		break;
	}
	// add component to the label
	if(scalar==SCALAR_VEL || scalar==SCALAR_ANGVEL){
		switch(vecAxis){
			case -1: lineColor->label="|"+lineColor->label+"|"; break;
			case 0: lineColor->label+="_x"; break;
			case 1: lineColor->label+="_y"; break;
			case 2: lineColor->label+="_z"; break;
		}
	}
	auto& dem=field->cast<DemField>();
	size_t i=0;
	bool matStateNameSet=false; // for SCALAR_MATSTATE, set at every step from the first matState available
	for(auto& n: dem.nodes){
		const auto& dyn(n->getData<DemData>());
		if(dyn.isTracerSkip()) continue;
		// node added
		if(!n->rep || !n->rep->isA<TraceVisRep>()){
			std::scoped_lock lock(dem.nodesMutex);
			n->rep=make_shared<TraceVisRep>();
			auto& tr=n->rep->cast<TraceVisRep>();
			tr.resize(num,saveTime);
			tr.flags=(compress>0?TraceVisRep::FLAG_COMPRESS:0) | (minDist>0?TraceVisRep::FLAG_MINDIST:0);
			tr.t0=scene->time;
		}
		auto& tr=n->rep->cast<TraceVisRep>();
		// update tracer pointer, if it is not us
		if(tr.tracer.get()!=this) tr.tracer=static_pointer_cast<Tracer>(shared_from_this());
		bool hasP=!dyn.parRef.empty();
		bool hidden=false;
		Real radius=NaN;
		if(modulo[0]>0 && (i+modulo[1])%modulo[0]!=0) hidden=true;
		if(dyn.isClump() && !clumps) hidden=true;
		// if the node has no particle, hide it when radius is the scalar
		if(!hasP && scalar==SCALAR_RADIUS) hidden=true;
		// get redius only when actually needed
		if(!hidden && (scalar==SCALAR_RADIUS || rRange.maxCoeff()>0)){
			const auto& pI(dyn.parRef.begin());
			if(hasP) radius=(*pI)->shape->equivRadius();
			else if(dyn.isClump()) radius=dyn.cast<ClumpData>().equivRad;
			if(rRange.maxCoeff()>0) hidden=(isnan(radius) || (rRange[0]>0 && radius<rRange[0]) || (rRange[1]>0 && radius>rRange[1]));
		}
		if(tr.isHidden()!=hidden) tr.setHidden(hidden);
		Real sc; // scalar by which the trace will be colored
		auto vecNormXyz=[&](const Vector3r& v)->Real{ if(vecAxis<0||vecAxis>2) return v.norm(); return v[vecAxis]; };
		switch(scalar){
			case SCALAR_VEL: sc=vecNormXyz(n->getData<DemData>().vel); break;
			case SCALAR_ANGVEL: sc=vecNormXyz(n->getData<DemData>().angVel); break;
			case SCALAR_SIGNED_ACCEL:{
				const auto& dyn=n->getData<DemData>();
				if(dyn.mass==0) sc=NaN;
				else sc=(dyn.vel.dot(dyn.force)>0?1:-1)*dyn.force.norm()/dyn.mass;
				break;
			}
			case SCALAR_RADIUS: sc=radius; break;
			case SCALAR_NUMCON: sc=(hasP?(*dyn.parRef.begin())->countRealContacts():NaN); break;
			case SCALAR_SHAPE_COLOR: sc=(hasP?(*dyn.parRef.begin())->shape->color:NaN); break;
			case SCALAR_TRACETIME: sc=scene->time-tr.t0; break; 
			case SCALAR_TIME: sc=scene->time; break;
			case SCALAR_ORDINAL: sc=(i%ordinalMod); break;
			case SCALAR_KINETIC: sc=dyn.getEk_any(n,/*trans*/true,/*rot*/true,scene); break;
			case SCALAR_MATSTATE:{
				if(hasP && dyn.parRef.front()->matState){
					const auto& mState=dyn.parRef.front()->matState;
					sc=mState->getScalar(matStateIx,scene->time,scene->step,matStateSmooth);
					if(!matStateNameSet){
						matStateNameSet=true;
						lineColor->label=mState->getScalarName(matStateIx);
					}
				} else sc=NaN;
				break;
			}
			default: sc=NaN;
		}
		tr.addPoint(n->pos,sc);
		i++;
	}

}
