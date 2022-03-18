#include"Outlet.hpp"
#include"Sphere.hpp"
#include"Clump.hpp"
#include"Funcs.hpp"
WOO_PLUGIN(dem,(Outlet)(BoxOutlet)(StackedBoxOutlet)(ArcOutlet));
WOO_IMPL_LOGGER(Outlet);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_Outlet__CLASS_BASE_DOC_ATTRS_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_BoxOutlet__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_StackedBoxOutlet__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_ArcOutlet__CLASS_BASE_DOC_ATTRS);


void Outlet::run(){
	DemField* dem=static_cast<DemField*>(field.get());
	Real stepMass=0.;
	std::set<std::tuple<Particle::id_t,int>> delParIdLoc;
	std::set<Particle::id_t> delClumpIxs;
	bool deleting=(markMask==0);
	auto canonPt=[this](const Vector3r& p)->Vector3r{ return scene->isPeriodic?scene->cell->canonicalizePt(p):p; };
	for(size_t i=0; i<dem->nodes.size(); i++){
		const auto& n=dem->nodes[i];
		int loc=-1, loc2=-1; // loc2 is for other nodes and ignored
		if(inside!=isInside(canonPt(n->pos),loc)) continue; // node inside, do nothing
		if(!n->hasData<DemData>()) continue;
		const auto& dyn=n->getData<DemData>();
		// check all particles attached to this node
		for(const Particle* p: dyn.parRef){
			if(!p || !p->shape) continue;
			// mask is checked for both deleting and marking
			if(mask && !(mask & p->mask)) continue;
			// marking, but mask has the markMask already
			if(!deleting && (markMask&p->mask)==markMask) continue;
			// check that all other nodes of that particle may also be deleted
			bool otherOk=true;
			for(const auto& nn: p->shape->nodes){
				// useless to check n again
				if(nn.get()!=n.get() && !(inside!=isInside(canonPt(nn->pos),loc2))){ otherOk=false; break; }
			}
			if(!otherOk) continue;
			LOG_TRACE("DemField.par[{}] marked for deletion.",i);
			delParIdLoc.insert(std::make_tuple(p->id,loc));
		}
		// if this is a clump, check positions of all attached nodes, and masks of their particles
		if(dyn.isClump()){
			assert(dynamic_pointer_cast<ClumpData>(n->getDataPtr<DemData>()));
			const auto& cd=n->getDataPtr<DemData>()->cast<ClumpData>();
			for(const auto& nn: cd.nodes){
				if(inside!=isInside(canonPt(nn->pos),loc2)) goto otherNotOk;
				for(const Particle* p: nn->getData<DemData>().parRef){
					assert(p);
					if(mask && !(mask & p->mask)) goto otherNotOk;
					if(!deleting && (markMask&p->mask)==markMask) goto otherNotOk;
					// don't check positions of all nodes of each particles, just assume that
					// all nodes of that particle are in the clump
				}
					
			}
			LOG_TRACE("DemField.nodes[{}]: clump marked for deletion, with all its particles.",i);
			delClumpIxs.insert(i);
			otherNotOk: ;
		}
	}
	for(const auto& idLoc: delParIdLoc){
		const auto& id(std::get<0>(idLoc)); const auto& loc(std::get<1>(idLoc));
		const shared_ptr<Particle>& p((*dem->particles)[id]);
		if(deleting && scene->trackEnergy) scene->energy->add(DemData::getEk_any(p->shape->nodes[0],true,true,scene),"kinOutlet",kinEnergyIx,EnergyTracker::ZeroDontCreate|EnergyTracker::IsIncrement,p->shape->nodes[0]->pos);
		const Real& m=p->shape->nodes[0]->getData<DemData>().mass;
		num++;
		mass+=m;
		stepMass+=m;
		// handle radius recovery with spheres
		if(recoverRadius && p->shape->isA<Sphere>()){
			auto& s=p->shape->cast<Sphere>();
			Real r=s.radius;
			if(recoverRadius){
				r=cbrt(3*m/(4*M_PI*p->material->density));
				rDivR0.push_back(s.radius/r);
				s.radius=r; // assign to the original value, so that savePar saves with the original diameter
			}
			if(save) diamMassTime.push_back(Vector3r(2*r,m,scene->time));
		} else{
			// all other cases
			if(save) diamMassTime.push_back(Vector3r(2*p->shape->equivRadius(),m,scene->time));
		}
		locs.push_back(loc);
		if(savePar) par.push_back(p);
		LOG_TRACE("DemField.par[{}] will be {}",id,(deleting?"deleted.":"marked."));
		if(deleting) dem->removeParticle(id);
		else p->mask|=markMask;
		LOG_DEBUG("DemField.par[{}] {}",id,(deleting?"deleted.":"marked."));
	}
	for(const auto& ix: delClumpIxs){
		const shared_ptr<Node>& n(dem->nodes[ix]);
		if(deleting && scene->trackEnergy) scene->energy->add(DemData::getEk_any(n,true,true,scene),"kinOutlet",kinEnergyIx,EnergyTracker::ZeroDontCreate|EnergyTracker::IsIncrement,canonPt(n->pos));
		Real m=n->getData<DemData>().mass;
		num++;
		mass+=m;
		stepMass+=m;
		if(save) diamMassTime.push_back(Vector3r(2*n->getData<DemData>().cast<ClumpData>().equivRad,m,scene->time));
		LOG_TRACE("DemField.nodes[{}] (clump) will be {}, with all its particles.",ix,(deleting?"deleted":"marked"));
		if(deleting) dem->removeClump(ix);
		else {
			// apply markMask on all clumps (all particles attached to all nodes in this clump)
			const auto& cd=n->getData<DemData>().cast<ClumpData>();
			for(const auto& nn: cd.nodes) for(Particle* p: nn->getData<DemData>().parRef) p->mask|=markMask;
		}
		LOG_TRACE("DemField.nodes[{}] (clump) {}",ix,(deleting?"deleted.":"marked."));
	}

	// use the whole stepPeriod for the first time (might be residuum from previous packing), if specified
	// otherwise the rate might be artificially high at the beginning
	Real currRateNoSmooth=stepMass/((((stepPrev<0 && stepPeriod>0)?stepPeriod:scene->step-stepPrev))*scene->dt);
	if(isnan(currRateNoSmooth) && scene->step>0) LOG_ERROR("currRateNoSmooth is NaN, with stepMass={}, stepPrev={}, stepPeriod={}, scene->step={}, scene->dt={}",stepMass,stepPrev,stepPeriod,scene->step,scene->dt);
	if(isnan(currRate)||stepPrev<0) currRate=currRateNoSmooth;
	else currRate=(1-currRateSmooth)*currRate+currRateSmooth*currRateNoSmooth;
}
py::object Outlet::pyDiamMass(bool zipped) const {
	return DemFuncs::seqVectorToPy(diamMassTime,/*itemGetter*/[](const Vector3r& i)->Vector2r{ return i.head<2>(); },/*zip*/zipped);
}
py::object Outlet::pyDiamMassTime(bool zipped) const {
	return DemFuncs::seqVectorToPy(diamMassTime,/*itemGetter*/[](const Vector3r& i)->Vector3r{ return i; },/*zip*/zipped);
}

Real Outlet::pyMassOfDiam(Real min, Real max) const {
	Real ret=0.;
	for(const auto& dm: diamMassTime){ if(dm[0]>=min && dm[0]<=max) ret+=dm[1]; }
	return ret;
}

py::object Outlet::pyPsd(bool _mass, bool cumulative, bool normalize, int _num, const Vector2r& dRange, const Vector2r& tRange, bool zip, bool emptyOk, const py::list& locs__){
	py::extract<vector<int>> ll(locs__);
	if(!ll.check()) throw std::runtime_error("Outlet.psd: locs must be a list of ints.");
	vector<int> locs_vec=ll();
	std::set<int> locs_set;
	for(auto& i: locs_vec) locs_set.insert(i);
	if(!save) throw std::runtime_error("Outlet.psd(): Outlet.save must be True.");
	auto tOk=[&tRange](const Real& t){ return isnan(tRange.minCoeff()) || (tRange[0]<=t && t<tRange[1]); };
	auto lOk=[&locs_set,this](const size_t& i){ return locs_set.empty() || (i<locs.size() && locs_set.count(locs[i])>0); };
	vector<Vector2r> psd=DemFuncs::psd(diamMassTime,cumulative,normalize,_num,dRange,
		/*diameter getter*/[&tOk,&lOk](const Vector3r& dmt, const size_t& i)->Real{ return (tOk(dmt[2]) && lOk(i))?dmt[0]:NaN; },
		/*weight getter*/[&_mass](const Vector3r& dmt, const size_t& i)->Real{ return _mass?dmt[1]:1.; },
		/*emptyOk*/ emptyOk
	);
	return DemFuncs::seqVectorToPy(psd,[](const Vector2r& i)->Vector2r{ return i; },/*zip*/zip);
}


#ifdef WOO_OPENGL
	void Outlet::renderMassAndRate(const Vector3r& pos){
		if(glHideZero && (isnan(currRate) || currRate==0) && mass!=0) return;
		std::ostringstream oss;
		oss.precision(4); oss<<mass;
		if(!isnan(currRate)){ oss.precision(3); oss<<"\n("<<currRate<<")"; }
		GLUtils::GLDrawText(oss.str(),pos,CompUtils::mapColor(glColor));
	}
#endif


#ifdef WOO_OPENGL
void BoxOutlet::render(const GLViewInfo&){
	if(isnan(glColor)) return;
	Vector3r center;
	if(!node){
		GLUtils::AlignedBox(box,CompUtils::mapColor(glColor));
		center=box.center();
	} else {
		glPushMatrix();
			GLUtils::setLocalCoords(node->pos,node->ori);
			GLUtils::AlignedBox(box,CompUtils::mapColor(glColor));
		glPopMatrix();
		center=node->loc2glob(box.center());
	}
	Outlet::renderMassAndRate(center);
}

void StackedBoxOutlet::render(const GLViewInfo& gli){
	if(isnan(glColor)) return;
	if(divColors.empty()){
		// one global box with darker divisors
		Vector3r color=.7*CompUtils::mapColor(glColor); // darker for subdivision
		if(!isnan(color.maxCoeff())){
			if(node){ glPushMatrix(); GLUtils::setLocalCoords(node->pos,node->ori); }
			glColor3v(color);
			short ax1((axis+1)%3),ax2((axis+2)%3);
			for(size_t d=0; d<divs.size(); d++){
				glBegin(GL_LINE_LOOP);
					Vector3r v;
					v[axis]=divs[d];
					for(size_t i=0; i<4; i++){
						v[ax1]=(i/2?box.min()[ax1]:box.max()[ax1]);
						v[ax2]=((!(i%2)!=!(i/2))?box.min()[ax2]:box.max()[ax2]); // XOR http://stackoverflow.com/a/1596970/761090
						glVertex3v(v);
					}
				glEnd();
			}
			if(node){ glPopMatrix(); }
			}
		BoxOutlet::render(gli); // render the rest
	} else { 
		// individual boxes with different colors
		if(node){ glPushMatrix(); GLUtils::setLocalCoords(node->pos,node->ori); }
		Real hair=box.sizes()[axis]*1e-4; // move edges by this much so that they are better visible
		for(size_t d=0; d<=divs.size(); d++){ // one extra; upper bound is always divs[d]
			Vector3r color=d<divColors.size()?CompUtils::mapColor(divColors[d]):Vector3r(.7*CompUtils::mapColor(glColor));
			AlignedBox3r b(box);
			if(d>0) b.min()[axis]=divs[d-1]+hair;
			if(d<divs.size()) b.max()[axis]=divs[d]-hair;
			GLUtils::AlignedBox(b,color);
		}
		if(node){ glPopMatrix(); }
		// render the rest, but no need to render the box again
		Outlet::renderMassAndRate(node?node->loc2glob(box.center()):box.center());
	}
}
#endif

void StackedBoxOutlet::postLoad(StackedBoxOutlet&, void* attr){
	if(!divs.empty()){
		for(size_t i=0; i<divs.size(); i++){
			if(isnan(divs[i]) || isinf(divs[i])) throw std::runtime_error("StackedBoxOutlet.divs: NaN and Inf not allowed (divs["+to_string(i)+"]="+to_string(divs[i])+").");
			if(i>0 && !(divs[i-1]<divs[i])) throw std::runtime_error("StackedBoxOutlet.divs: must be increasing (divs["+to_string(i-1)+"]="+to_string(divs[i-1])+", divs["+to_string(i)+"]="+to_string(divs[i])+").");
		}
	}
}

bool StackedBoxOutlet::isInside(const Vector3r& p, int& loc){
	Vector3r pp(node?node->glob2loc(p):p);
	if(!box.contains(pp)) return false;
	// find which stack in the box
	loc=std::upper_bound(divs.begin(),divs.end(),pp[axis])-divs.begin();
	loc+=loc0; // add starting index
	return true;
}


void ArcOutlet::postLoad(ArcOutlet&, void* attr){
	if(!cylBox.isEmpty() && (cylBox.min()[0]<0 || cylBox.max()[0]<0)) throw std::runtime_error("ArcOutlet.cylBox: radius bounds (x-component) must be non-negative (not "+to_string(cylBox.min()[0])+".."+to_string(cylBox.max()[0])+").");
	if(!node){ node=make_shared<Node>(); throw std::runtime_error("ArcOutlet.node: must not be None (dummy node created)."); }
};

bool ArcOutlet::isInside(const Vector3r& p, int& loc){ return CompUtils::cylCoordBox_contains_cartesian(cylBox,node->glob2loc(p)); }
#ifdef WOO_OPENGL
	void ArcOutlet::render(const GLViewInfo&){
		if(isnan(glColor)) return;
		glPushMatrix();
			GLUtils::setLocalCoords(node->pos,node->ori);
			GLUtils::RevolvedRectangle(cylBox,CompUtils::mapColor(glColor),glSlices);
		glPopMatrix();
		Outlet::renderMassAndRate(node->loc2glob(CompUtils::cyl2cart(cylBox.center())));
	}
#endif
