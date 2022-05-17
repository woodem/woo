// 2009 © Václav Šmilauer <eudoxos@arcig.cz> 

#include"InsertionSortCollider.hpp"
#include"ParticleContainer.hpp"
#include"../core/Scene.hpp"

#include<algorithm>
#include<vector>
#include<boost/static_assert.hpp>
#include<boost/algorithm/string/join.hpp>

#if __has_include(<execution>)
	#include<execution>
#endif

#if defined(WOO_OPENMP) && defined(__GNUC__) && !defined(__clang__)
	#include<parallel/algorithm>
#endif


#define PERI_INIT_FIX1
//#define PERI_INIT_FIX2
#define ISC_PERI_CHUNKED_IS

#if !(defined(PERI_INIT_FIX1) || defined(PERI_INIT_FIX2))
	#error At least one of PERI_INIT_FIX1, PERI_INIT_FIX2 *must* be defined (initial sort with inifite boxes)
#endif


WOO_PLUGIN(dem,(InsertionSortCollider));
WOO_IMPL_LOGGER(InsertionSortCollider);

WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR_PY(woo_dem_InsertionSortCollider__CLASS_BASE_DOC_ATTRS_CTOR_PY);

void InsertionSortCollider::makeRemoveContactLater_process() {

	#if defined(WOO_OPENMP) || defined(WOO_OPENGL)
		std::scoped_lock lock(dem->contacts->manipMutex);
	#endif

	ISC_CHECKPOINT("later: start");


	#ifdef WOO_OPENMP
		for(auto& removeContacts: rremoveContacts){
	#endif
			for(const auto& C: removeContacts) dem->contacts->removeMaybe_fast(C);
			removeContacts.clear();
	#ifdef WOO_OPENMP
		}
	#endif

	ISC_CHECKPOINT("later: remove");
	#ifdef WOO_OPENMP
		for(auto & makeContacts: mmakeContacts){
	#endif
			for(const auto& C: makeContacts)	dem->contacts->addMaybe_fast(C);
			makeContacts.clear();
	#ifdef WOO_OPENMP
		}
	#endif
	ISC_CHECKPOINT("later: add");
};


// return true if bodies bb overlap in all 3 dimensions
bool InsertionSortCollider::spatialOverlap(Particle::id_t id1, Particle::id_t id2) const {
	assert(!periodic);
	return
		(minima[3*id1+0]<=maxima[3*id2+0]) && (maxima[3*id1+0]>=minima[3*id2+0]) &&
		(minima[3*id1+1]<=maxima[3*id2+1]) && (maxima[3*id1+1]>=minima[3*id2+1]) &&
		(minima[3*id1+2]<=maxima[3*id2+2]) && (maxima[3*id1+2]>=minima[3*id2+2]);
}



// called by the insertion sort if 2 bodies swapped their bounds
void InsertionSortCollider::handleBoundInversion(Particle::id_t id1, Particle::id_t id2, bool separating){
	assert(!periodic);
	assert(id1!=id2);
	// do bboxes overlap in all 3 dimensions?
	ISC_CHECKPOINT("handle: start");
	bool overlap=separating?false:spatialOverlap(id1,id2); // if bboxes seaprate, there is for sure no overlap
	ISC_CHECKPOINT("handle: overlap-test");
	// existing interaction?
	const shared_ptr<Contact>& C=dem->contacts->find(id1,id2);
	ISC_CHECKPOINT("handle: find-contact");
	bool hasCon=(bool)C;
	// interaction doesn't exist and shouldn't, or it exists and should
	if(WOO_LIKELY(!overlap && !hasCon)) return;
	if(overlap && hasCon){  return; }
	// create interaction if not yet existing
	if(overlap /* && !hasCon */){ // second condition redundant
		const shared_ptr<Particle>& p1((*particles)[id1]);
		const shared_ptr<Particle>& p2((*particles)[id2]);
		if(!Collider::mayCollide(dem,p1,p2)) return;
		#if 0
			// reorder for clDem compatibility; otherwise not needed
			if(id1<id2) makeContactLater(p1,p2,Vector3i::Zero());
			else makeContactLater(p2,p1,Vector3i::Zero());
		#else
			makeContactLater(p1,p2,Vector3i::Zero());
			ISC_CHECKPOINT("handle: later-new");
		#endif
		return;
	}
	if(!overlap /* && hasCon */){
		if(!C->isReal()) removeContactLater(C);
		ISC_CHECKPOINT("handle: later-remove");
		return;
	}
	assert(false); // unreachable
}

void InsertionSortCollider::throwTooManyPasses(){
	string m=
		maxSortPass>0
		?fmt::format("maxSortPass={} partial sort passes",maxSortPass)
		:fmt::format("{} partial sort passes (maxSortPass={} with {} OpenMP threads)",-maxSortPass*omp_get_max_threads(),maxSortPass,omp_get_max_threads())
	;
	throw std::runtime_error("InsertionSortCollider: sort not done after "+m+". If motion is out-of-control, increasing Scene.dtSafety might help next time.");
}

void InsertionSortCollider::insertionSort(VecBounds& v, bool doCollide, int ax){
	assert(!periodic);
	assert(v.size==(long)v.vec.size());
	if(v.size==0) return;

	#ifndef WOO_OPENMP
		insertionSort_part(v,doCollide,ax,0,v.size,0);
	#else
		// will be adapted -- can be the double and so on
		int chunks_per_core=ompTuneSort[0];
		int min_per_core=2*ompTuneSort[1]; // ×2: number of particles → number of bounds;
		int max_per_core=2*ompTuneSort[2];
		int TH=omp_get_max_threads();
		size_t chunks=TH*max(1,chunks_per_core);
		// apply bounds, but always round up to a multiple of TH
		if(min_per_core>0 && (int)(v.size/chunks)<min_per_core) chunks=TH*(int)ceil(max(1L,v.size/min_per_core)*1./TH);
		else if(max_per_core>0 && (int)(v.size/chunks)>max_per_core) chunks=TH*(int)ceil(max(1L,v.size/max_per_core)*1./TH);
		size_t chunkSize=v.size/chunks;
		// don't bother with parallelized sorting for small number of bounds
		if(chunks==1){ insertionSort_part(v,doCollide,ax,0,v.size,0); return; }
		if(chunks==0) throw std::logic_error("0 chunks for parallel insertion sort!");
		sortChunks=chunks; // for diagnostics

		// too small chunks, go sequential
		if(chunkSize<100){ insertionSort_part(v,doCollide,ax,0,v.size,0); return; }

		// pre-compute split points
		/*
			============================================= bound sequence
			              with chunks==4:
			|----------|----------|----------|----------| splits0 (even sorts run in-between)
			     |----------|-----------|----------|      splits1 (odd sorts run in-between)

			Split-points inner to bounds are checked for ordering after each pass;
			when all of them are ordered, it means the whole sequence is ordered.

		*/
		vector<size_t> splits0(chunks+1), splits1(chunks);
		for(size_t i=0; i<chunks; i++){ splits0[i]=i*chunkSize; splits1[i]=i*chunkSize+chunkSize/2; }
		splits0[chunks]=v.size;
		bool isOk=false; size_t pass;
		size_t maxPass=maxSortPass>0?maxSortPass:(-maxSortPass*omp_get_max_threads());
		for(pass=0; !isOk; pass++){
			if(pass==maxPass) throwTooManyPasses();
			bool even=((pass%2)==0);
			const vector<size_t>& s(even?splits0:splits1);
			#pragma omp parallel for schedule(static)
			for(size_t chunk=0; chunk<(even?chunks:chunks-1); chunk++){
				// start at the beginning of the chunk in the first pass;
				// start in the mid-split in subsequent passes
				size_t start(pass==0?s[chunk]:(even?splits1[chunk]:splits0[chunk+1]));
				insertionSort_part(v,doCollide,ax,s[chunk],s[chunk+1],start);
			}
			// check boundaries between chunks -- if all of them are ordered, we're done; otherwise a next pass is needed
			isOk=true;
			for(size_t chunk=(even?1:0); chunk<chunks; chunk++){
				if(v[s[chunk]-1]>v[s[chunk]]){ isOk=false; break; }
			}
		}
		// cerr<<"Parallel insertion sort done ("<<pass+1<<") passes, "<<chunks<<" chunks; inversions remaining: "<<countInversions().transpose()<<endl;
		// cerr<<"Parallel insertion sort done, needed "<<pass+1<<" passes."<<endl;
	#endif
}

// perform partial insertion sort
/*__attribute__((optimize("O0")))*/ void InsertionSortCollider::insertionSort_part(VecBounds& v, bool doCollide, int ax, long iBegin, long iEnd, long iStart){
	// stop after encountering the first ordered couple; this is used from the parallel sort
	const bool earlyStop=(iBegin!=iStart);

	// the max(...) condition ensures v[j=i-1] is initially valid
	// otherwise the while(... && j>=iBegin) stops right away anyway
	for(long i=max(iStart,iBegin+1); i<iEnd; i++){
		// no inversion here, short-circuit the whole walking setup and avoid the write after the while loop
		if(!(v[i-1]>v[i])){
			if(WOO_UNLIKELY(earlyStop)){ /*cerr<<"{"<<i-iStart<<"}";*/ return; }
			continue;
		}

		const Bounds viInit=v[i]; long j=i-1; /* cache hasBB; otherwise 1% overall performance hit */ const bool viInitBB=viInit.flags.hasBB; const bool viInitIsMin=viInit.flags.isMin;
		while(j>=iBegin && v[j]>viInit){ // first check whether j is valid, not the other way around
			v[j+1]=v[j];
			#ifdef PISC_DEBUG
				if(watchIds(v[j].id,viInit.id)) cerr<<"Swapping #"<<v[j].id<<"  with #"<<viInit.id<<" ("<<std::setprecision(80)<<v[j].coord<<">"<<std::setprecision(80)<<viInit.coord<<" along axis "<<v.axis<<")"<<endl;
				if(v[j].id==viInit.id){ cerr<<"Inversion of #"<<v[j].id<<" with itself, "<<v[j].flags.isMin<<" & "<<viInit.flags.isMin<<", isGreater "<<(v[j]>viInit)<<", "<<(v[j].coord>viInit.coord)<<endl; j--; continue; }
			#endif
			#ifdef WOO_DEBUG
				stepInvs[ax]++; numInvs[ax]++;
			#endif
			// no collisions without bounding boxes
			// also, do not collide particle with itself; it sometimes happens for facets aligned perpendicular to an axis, for reasons that are not very clear
			// see https://bugs.launchpad.net/woo/+bug/669095
			// do not collide minimum with minimum and maximum with maximum (suggested by Bruno)
			if(viInitIsMin!=v[j].flags.isMin && WOO_LIKELY(doCollide && viInitBB && v[j].flags.hasBB && (viInit.id!=v[j].id))){
				handleBoundInversion(viInit.id,v[j].id,/*separating*/!viInitIsMin);
			}
			j--;
		}
		v[j+1]=viInit;
	}
}

Vector3i InsertionSortCollider::countInversions(){
	Vector3i ret;
	for(int ax:{0,1,2}){
		int N=0;
		const VecBounds& v(BB[ax]);
		for(long i=0; i<v.size-1; i++){
			for(long j=i+1; j<v.size; j++){
				if(v[i]>v[j]){
					N++;
				} 
			}
		}
		ret[ax]=N;
	}
	return ret;
}

// if(verletDist>0){ mn-=verletDist*Vector3r::Ones(); mx+=verletDist*Vector3r::Ones(); }
vector<Particle::id_t> InsertionSortCollider::probeAabb(const Vector3r& mn, const Vector3r& mx){
	vector<Particle::id_t> ret;
	const short ax0=0; // use the x-axis for the traversal
	const VecBounds& v(BB[ax0]);
	if(!periodic){
		#if 0
			auto I=std::lower_bound(v.vec.begin(),v.vec.end(),mn[ax0],[](const Bounds& b, const Real& c)->bool{ return b.coord<c; } );
			#ifdef WOO_DEBUG
				long i=I-v.vec.begin();
				long pi=max(0L,i-1), ppi=max(0L,i-2), ni=min((long)v.vec.size(),i+1), nni=min((long)v.vec.size(),i+2);
				cerr<<"x="<<mn[ax0]<<":: b["<<ppi<<"]="<<v.vec[ppi].coord<<", b["<<pi<<"]="<<v.vec[pi].coord<<" || "<<v.vec[i].coord<<" || b["<<ni<<"]="<<v.vec[ni].coord<<", b["<<nni<<"]="<<v.vec[nni].coord<<endl;
			#endif
		#endif
		for(long i=0; i<v.size; i++){
			const Bounds& b(v.vec[i]);
			if(!b.flags.isMin || !b.flags.hasBB) continue;
			if(b.coord>mx[ax0]) break;
			long off=3*b.id;
			//Particle::id_t id2=I->id;
			bool overlap=
				(mn[0]<=maxima[off+0]) && (mx[0]>=minima[off+0]) &&
				(mn[1]<=maxima[off+1]) && (mx[1]>=minima[off+1]) &&
				(mn[2]<=maxima[off+2]) && (mx[2]>=minima[off+2]);
			if(overlap) ret.push_back(b.id);
		}
		return ret;
	} else {
		// for the periodic case, go through all particles
		// algorithmically wasteful :|
		// copied from handleBoundInversionPeri, with adjustments
		for(long i=0; i<v.size; i++){
			// safely skip deleted particles to avoid spurious overlaps
			const Bounds& b(v.vec[i]);
			if(!b.flags.isMin || !b.flags.hasBB) continue;
			long id1=b.id;
			for(int axis=0; axis<3; axis++){
				Real dim=scene->cell->getSize()[axis];
				// find particle of which minimum when taken as period start will make the gap smaller
				Real m1=minima[3*id1+axis],m2=mn[axis];
				Real wMn=(cellWrapRel(m1,m2,m2+dim)<cellWrapRel(m2,m1,m1+dim)) ? m2 : m1;
				int pmn1,pmx1,pmn2,pmx2;
				Real mn1=cellWrap(minima[3*id1+axis],wMn,wMn+dim,pmn1), mx1=cellWrap(maxima[3*id1+axis],wMn,wMn+dim,pmx1);
				Real mn2=cellWrap(mn[axis],wMn,wMn+dim,pmn2), mx2=cellWrap(mx[axis],wMn,wMn+dim,pmx2);
				if(WOO_UNLIKELY((pmn1!=pmx1) || (pmn2!=pmx2))){
					Real span=(pmn1!=pmx1?mx1-mn1:mx2-mn2); if(span<0) span=dim-span;
					LOG_FATAL("Particle #{} spans over half of the cell size {} (axis={}, min={}, max={}, span={})",(pmn1!=pmx1?id1:-1),dim,axis,(pmn1!=pmx1?mn1:mn2),(pmn1!=pmx1?mx1:mx2),span);
					throw runtime_error(__FILE__ ": Particle larger than half of the cell size encountered.");
				}
				if(!(mn1<=mx2 && mx1 >= mn2)) goto noOverlap;
			}
			ret.push_back(id1);
			noOverlap: ;
		}
		return ret;
	}
};

// STRIDE
bool InsertionSortCollider::isActivated(){
	// we wouldn't run in this step; in that case, just delete pending interactions
	// this is done in ::action normally, but it would make the call counters not reflect the stride
	return true;
}


bool InsertionSortCollider::updateBboxes_doFullRun(){
	ISC_CHECKPOINT("bounds: start");
	// update bounds via boundDispatcher
	AabbCollider::initBoundDispatcher();
	
	AabbCollider::setVerletDist(scene,dem);
	assert(verletDist>=0);

	bool recomputeBounds=false;
	if(verletDist==0){
		recomputeBounds=true;
	} else {
		// first loop only checks if there something is our
		for(const shared_ptr<Particle>& p: *dem->particles){
			if(!p->shape) continue;
			if(AabbCollider::aabbIsDirty(p)){
				recomputeBounds=true;
				break;
			}
		}
	}
	ISC_CHECKPOINT("bounds: check");
	// bounds don't need update, collision neither
	if(!recomputeBounds) return false;


	// this loop takes 25% collider time when not parallelized, give it a try
	size_t size=particles->size();
	/*
		HACK: there are some (reproducible) crashes in the ctor of Aabb when this is run in parallel
		for the first time. This is not caused by calls to createIndex() in Aabb or Bound ctors
		(both checked, simultaneously and separately), and does not occus when not running in parallel.

		This occurs only with some particular simulations, such as examples/facet-facet.py.

		It is possibly related to some race conditions in the BoundDispatcher, and should be
		examined more closely.

		Therefore, always run serially for the very first time, until a better solution is found.
	*/
	#ifdef WOO_OPENMP
		#pragma omp parallel for num_threads(nFullRuns>0?omp_get_max_threads():1)
	#endif
	for(size_t i=0; i<size; i++){
		const shared_ptr<Particle>& p((*particles)[i]);
		if(!p || !p->shape) continue;
		AabbCollider::updateAabb(p);
	}
	ISC_CHECKPOINT("bounds: recompute");
	return true;
}

bool InsertionSortCollider::prologue_doFullRun(){
	dem=dynamic_cast<DemField*>(field.get());
	assert(dem);
	particles=dem->particles.get();

	// scene->interactions->iterColliderLastRun=-1;

	// conditions when we need to run a full pass
	bool fullRun=false;

	// contacts are dirty and must be detected anew
	if(dem->contacts->dirty || forceInitSort){ fullRun=true; dem->contacts->dirty=false; }

	// number of particles changed
	if((size_t)BB[0].size!=2*particles->size()) fullRun=true;
	//redundant: if(minima.size()!=3*nPar || maxima.size()!=3*nPar) fullRun=true;

	// periodicity changed
	if(scene->isPeriodic != periodic){
		for(int i=0; i<3; i++) BB[i].clear();
		periodic=scene->isPeriodic;
		fullRun=true;
	}
	return fullRun;
}

void InsertionSortCollider::run(){
	#ifdef ISC_TIMING
		timingDeltas->start();
	#endif
	ISC_CHECKPOINT("== start ==");
	bool fullRun=prologue_doFullRun();
	ISC_CHECKPOINT("prologue");
	bool runBboxes=updateBboxes_doFullRun();
	ISC_CHECKPOINT("bbox-check");

	if(runBboxes) fullRun=true;

	stepInvs=Vector3i::Zero();

	if(!fullRun){
		// done in every step; with fullRun, number of particles might have changed
		// in that case, this is called below, not here
		field->cast<DemField>().contacts->removePending(*this,scene);
		return;
	}

	nFullRuns++;

	long nPar=(long)particles->size();

	// pre-conditions
		// adjust storage size
		bool doInitSort=false;
		if(forceInitSort){ doInitSort=true; forceInitSort=false; }
		assert(BB[0].size==BB[1].size); assert(BB[1].size==BB[2].size);
		if(BB[0].size!=2*nPar){
			LOG_DEBUG("Resize bounds containers from {} to {}, will std::sort.",BB[0].size,nPar*2);
			// bodies deleted; clear the container completely, and do as if all bodies were added (rather slow…)
			// future possibility: insertion sort with such operator that deleted bodies would all go to the end, then just trim bounds
			if(2*nPar<BB[0].size){ for(int i: {0,1,2}){ BB[i].clear(); }}
			// more than 100 bodies was added, do initial sort again
			// maybe: should rather depend on ratio of added bodies to those already present...?
			else if(2*nPar-BB[0].size>200 || BB[0].size==0) doInitSort=true;
			assert((BB[0].size%2)==0);
			for(int i:{0,1,2}){
				BB[i].vec.reserve(2*nPar);
				// add lower and upper bounds; coord is not important, will be updated from bb shortly
				for(long id=BB[i].vec.size()/2; id<nPar; id++){ BB[i].vec.push_back(Bounds(0,id,/*isMin=*/true)); BB[i].vec.push_back(Bounds(0,id,/*isMin=*/false)); }
				BB[i].size=BB[i].vec.size();
				assert(BB[i].size==2*nPar);
				assert(nPar==(long)particles->size());
			}
		}
		if(minima.size()!=(size_t)3*nPar){ minima.resize(3*nPar); maxima.resize(3*nPar); }
		assert((size_t)BB[0].size==2*particles->size());

		// update periodicity
		assert(BB[0].axis==0); assert(BB[1].axis==1); assert(BB[2].axis==2);
		if(periodic) for(int i=0; i<3; i++) BB[i].updatePeriodicity(scene);

	#if 0
		// if interactions are dirty, force reinitialization
		if(scene->interactions->dirty){
			doInitSort=true;
			scene->interactions->dirty=false;
		}
	#endif


	ISC_CHECKPOINT("bound");

	// copy bounds along given axis into our arrays
	// this loop takes 27% of total collider time, try to make it parallel
		#ifdef WOO_OPENMP
			#pragma omp parallel for schedule(static)
		#endif
		for(long i=0; i<2*nPar; i++){
			for(int j=0; j<3; j++){
				VecBounds& BBj=BB[j];
				const Particle::id_t id=BBj[i].id;
				const shared_ptr<Particle>& b=(*particles)[id];
				if(WOO_LIKELY(b)){
					const shared_ptr<Bound>& bv=b->shape->bound;
					// coordinate is min/max if has bounding volume, otherwise both are the position. Add periodic shift so that we are inside the cell
					// watch out for the parentheses around ?: within ?: (there was unwanted conversion of the Reals to bools!)
					BBj[i].coord=((BBj[i].flags.hasBB=((bool)bv)) ? (BBj[i].flags.isMin ? bv->min[j] : bv->max[j]) : (b->shape->nodes[0]->pos[j])) - (periodic ? BBj.cellDim*BBj[i].period : 0.);
					BBj[i].flags.isInf=false;
					if(periodic && isinf(BBj[i].coord)){
						// check that min and max are both infinite or none of them
						assert(!bv || ((isinf(bv->min[j]) && isinf(bv->max[j])) || (!isinf(bv->min[j]) && !isinf(bv->max[j]))));
						#ifdef PERI_INIT_FIX1
							/* this was a good idea but initial sort diregards the period, thus fails to detect initial contact with the infinite BB */
							BBj[i].coord=(BBj[i].flags.isMin?0:BBj.cellDim);
						#else
							// infinite particles span between cell boundaries (both have coordinate 0), the lower bound having period 0, the upper one 1 (does not change during simulation at all)
							BBj[i].period=(BBj[i].flags.isMin?0:1); BBj[i].coord=0.; /* wasInf=true; */
						#endif
						BBj[i].flags.isInf=true; // keep track of infinite coord here, so that we know there is no separation possible later
					}
				} else { // vanished particle
					BBj[i].flags.hasBB=false;
					// when doing initial sort, set to -inf so that nonexistent particles don't generate inversions later
					// for periodic, use zero, since -Inf would make that particle move through all other every time
					// slowing the computation down by two orders of magnitude
					if(doInitSort) BBj[i].coord=(periodic?0:-Inf);
					// otherwise keep the coordinate as-is, to minimize inversions
				}
				// if initializing periodic, shift coords & record the period into BBj[i].period
				if(doInitSort && periodic) {
					// don't do this for infinite bbox which was adjusted to the cell size above
					if(!BBj[i].flags.isInf) BBj[i].coord=cellWrap(BBj[i].coord,0,BBj.cellDim,BBj[i].period);
				}
			}	
		}
	ISC_CHECKPOINT("copy-bounds");

	// for each body, copy its minima and maxima, for quick checks of overlaps later
	for(Particle::id_t id=0; id<nPar; id++){
		BOOST_STATIC_ASSERT(sizeof(Vector3r)==3*sizeof(Real));
		const shared_ptr<Particle>& b=(*particles)[id];
		if(WOO_LIKELY(b)){
			const shared_ptr<Bound>& bv=b->shape->bound;
			if(WOO_LIKELY(bv)) { memcpy(&minima[3*id],&bv->min,3*sizeof(Real)); memcpy(&maxima[3*id],&bv->max,3*sizeof(Real)); } // ⇐ faster than 6 assignments 
			else{ const Vector3r& pos=b->shape->nodes[0]->pos; memcpy(&minima[3*id],&pos,3*sizeof(Real)); memcpy(&maxima[3*id],&pos,3*sizeof(Real)); }
		} else { memset(&minima[3*id],0,3*sizeof(Real)); memset(&maxima[3*id],0,3*sizeof(Real)); }
	}
	ISC_CHECKPOINT("copy-minima-maxima");

	#if 1 && defined(WOO_OPENMP)
		for(const auto& pending: field->cast<DemField>().contacts->threadsPending){
			for(const auto& p: pending){
				if(!p.contact) LOG_FATAL("Pending-removal contact is NULL?!");
			}
		}
	#endif
	// process interactions that the constitutive law asked to be erased
	field->cast<DemField>().contacts->removePending(*this,scene);

	ISC_CHECKPOINT("erase");
	ISC_CHECKPOINT("pre-sort");

	// sort
		// the regular case
		if(!doInitSort && !sortThenCollide){
			/* each inversion in insertionSort calls handleBoundInversion, which in turns may add/remove interaction */
			if(!periodic) for(int i:{0,1,2}){
				//Vector3i invs=countInversions();
				insertionSort(BB[i],/*collide*/true,i); 
				//LOG_INFO("{}/{} invs (counted/insertion sort)",invs.sum(),stepInvs);
			}
			else for(int i:{0,1,2}) insertionSortPeri(BB[i],/*collide*/true,i);
			ISC_CHECKPOINT("insertion-sort-done");
		}
		// create initial interactions (much slower)
		else {
			if(doInitSort){
				// the initial sort is in independent in 3 dimensions, may be run in parallel; it seems that there is no time gain running in parallel, though
				// important to reset loInx for periodic simulation (!!)
				LOG_DEBUG("Initial std::sort over all axes");
				for(int i:{0,1,2}) {
					#ifdef __cpp_lib_execution
						std::sort(std::execution::par,BB[i].vec.begin(),BB[i].vec.end());
					#else
						#if defined(WOO_OPENMP) && defined(__GNUC__) && !defined(__INTEL_COMPILER) && !defined(__clang__)
							__gnu_parallel::sort(BB[i].vec.begin(),BB[i].vec.end());
						#else
							#ifdef WOO_OPENMP
								#warning Clang and Intel compilers will use non-parallel sort; compile with c++17 and <execution> header for std::sort parallel algorithm, with gcc (will uses __gnu_parallel::sort)
							#endif
							std::sort(BB[i].vec.begin(),BB[i].vec.end());
						#endif
					#endif
				}
				numReinit++;
			} else { // sortThenCollide
				if(!periodic) for(int i:{0,1,2}) insertionSort(BB[i],false,i);
				else for(int i:{0,1,2}) insertionSortPeri(BB[i],false,i);
			}
			ISC_CHECKPOINT("init-sort-done");

			// traverse the container along requested axis
			assert(sortAxis==0 || sortAxis==1 || sortAxis==2);
			VecBounds& V=BB[sortAxis];

			// go through potential aabb collisions, create contacts as necessary
			if(!periodic){
				// parallelizing this decreases performance slightly
				for(long i=0; i<2*nPar; i++){
					// start from the lower bound (i.e. skipping upper bounds)
					// skip bodies without bbox, because they don't collide
					if(WOO_UNLIKELY(!(V[i].flags.isMin && V[i].flags.hasBB))) continue;
					const Particle::id_t& iid=V[i].id;
					// go up until we meet the upper bound
					for(long j=i+1; /* handle case 2. of swapped min/max */ j<2*nPar && V[j].id!=iid; j++){
						const Particle::id_t& jid=V[j].id;
						// take 2 of the same condition (only handle collision [min_i..max_i]+min_j, not [min_i..max_i]+min_i (symmetric)
						if(!V[j].flags.isMin) continue;
						/* abuse the same function here; since it does spatial overlap check first (with separating==false), it is OK to use it */
						handleBoundInversion(iid,jid,/*separting*/false);
						assert(j<=2*nPar-1); // XXX: should this really be j<2*nPar-1?? 
					}
				}
			} else { // periodic case: see comments above
				// parallelizing this decreases performance slightly
				for(long i=0; i<2*nPar; i++){
					if(WOO_UNLIKELY(!(V[i].flags.isMin && V[i].flags.hasBB))){ LOG_DEBUG("Skipping {} (id={}, isMin={})",i,V[i].id,V[i].flags.isMin); continue; }
					const Particle::id_t& iid=V[i].id;
					long cnt=0;
					// we might wrap over the periodic boundary here; that's why the condition is different from the aperiodic case
					LOG_TRACE("i={}, loop: j=V.norm(i+1)={}; V[j].id={} != iid={}; j=V.norm(j+1)",i,V.norm(i+1),V[V.norm(i+1)].id,iid)
					for(long j=V.norm(i+1); V[j].id!=iid
						#ifdef PERI_INIT_FIX2
							|| (V[i].flags.isInf && !V[j].flags.isMin)
						#endif
							; j=V.norm(j+1)){
						LOG_TRACE("i={} (id={}, isMin={}), j={} (id={}, isMin={})",i,V[i].id,V[i].flags.isMin,j,V[j].id,V[j].flags.isMin);
						if(cnt++>2*(long)nPar){ LOG_FATAL("Uninterrupted loop in the initial sort?"); throw std::logic_error("loop??"); }
						const Particle::id_t& jid=V[j].id;
						if(!V[j].flags.isMin) continue;
						LOG_TRACE("Calling handleBoundInversionPeri({},{},false)",iid,jid)
						handleBoundInversionPeri(iid,jid,/*separating*/false);
					}
				}
			}
			ISC_CHECKPOINT("init-contacts-done");
		}
	ISC_CHECKPOINT("sort&collide");

	makeRemoveContactLater_process();

	ISC_CHECKPOINT("make-remove-write");
}


// return floating value wrapped between x0 and x1 and saving period number to period
Real InsertionSortCollider::cellWrap(const Real x, const Real x0, const Real x1, int& period){
	Real xNorm=(x-x0)/(x1-x0);
	period=(int)floor(xNorm); // some people say this is very slow; probably optimized by gcc, however (google suggests)
	return x0+(xNorm-period)*(x1-x0);
}

// return coordinate wrapped to 0…(x1-x0), relative to x0, and save period
Real InsertionSortCollider::cellWrapRel(const Real x, const Real x0, const Real x1, int& period){
	Real xNorm=(x-x0)/(x1-x0);
	period=(int)floor(xNorm);
	return (xNorm-floor(xNorm))*(x1-x0);
}

// return coordinate wrapped to 0…(x1-x0), relative to x0; don't care about period
Real InsertionSortCollider::cellWrapRel(const Real x, const Real x0, const Real x1){
	Real xNorm=(x-x0)/(x1-x0);
	return (xNorm-floor(xNorm))*(x1-x0);
}

#ifdef ISC_PERI_CHUNKED_IS
void InsertionSortCollider::insertionSortPeri(VecBounds& v, bool doCollide, int ax){
	assert(periodic);
	assert(v.size==(long)v.vec.size());
	if(v.size==0) return;
//	#ifndef WOO_OPENMP
//		//insertionSortPeri_part(v,doCollide,ax,0,v.size,0);
//		// insertionSortPeri_orig(v,doCollide,ax);
//	#else
	#ifndef WOO_OPENMP
		const int chunks=1;
		const int chunkSize=v.size;
	#else
		// one chunk per core; the complicated logic for the non-periodic variant has not brought any improvement
		int chunks=omp_get_max_threads();
		int chunkSize=v.size/chunks;
		if(chunkSize<100 || !paraPeri){
			//if(v.size>0) insertionSortPeri_part(v,doCollide,ax,0,v.size,0);
			// insertionSortPeri_orig(v,doCollide,ax);
			//return;
			chunks=1;
			chunkSize=v.size;
		}
	#endif

		/*
			============================================= bound sequence
			              with chunks==4:
			0----------1----------2----------3----------4         splits0 (even sorts run in-between)
			.....0----------1-----------2----------3-----.....4   splits1 (odd sorts run in-between); 4 wraps to 0

			Split-points inner to bounds are checked for ordering after each pass;
			when all of them are ordered, it means the whole sequence is ordered.

		*/
		vector<size_t> splits0(chunks+1), splits1(chunks+1);
		for(int i=0; i<chunks; i++){ splits0[i]=i*chunkSize; splits1[i]=i*chunkSize+chunkSize/2; }
		splits0[chunks]=v.size;
		splits1[chunks]=splits1[0]+v.size; // wrapping around
		//cerr<<"splits0:"; for(auto s: splits0) cerr<<" "<<s; cerr<<endl;
		//cerr<<"splits1:"; for(auto s: splits1) cerr<<" "<<s; cerr<<endl;
		
		size_t maxPass=maxSortPass>0?maxSortPass:(-maxSortPass*omp_get_max_threads());

		bool isOk=false; size_t pass;
		for(pass=0; !isOk; pass++){
			if(pass==maxPass) throwTooManyPasses();
			bool even=((pass%2)==0);
			const vector<size_t>& s(even?splits0:splits1);
			#ifdef WOO_OPENMP
				#pragma omp parallel for schedule(static)
			#endif
			for(int chunk=0; chunk<chunks; chunk++){
				long start(pass==0?s[chunk]:(even?splits1[chunk]:splits0[chunk+1]));
				// cerr<<"{pass:"<<pass<<",chunk:"<<chunk<<"/"<<chunks-1<<":"<<s[chunk]<<","<<s[chunk+1]<<","<<start<<"}";
				insertionSortPeri_part(v,doCollide,ax,s[chunk],s[chunk+1],start);
			}
			isOk=true;
			// if(pass<=3) isOk=false;
			for(int chunk=0; chunk<chunks; chunk++){
				// cerr<<"["<<chunk<<"/"<<chunks-1<<"]";
				long i=v.norm(s[chunk]); long i_1=v.norm(i-1);
				// this condition is copied over from the InsertionSortPeri_part loop
				if((i==v.loIdx && v[i].coord<0) || v[i_1].coord>v[i].coord+(i==v.loIdx?v.cellDim:0)){ isOk=false; break; }
			}
		}
		// cerr<<" (("<<pass<<" passes))"<<endl;
		// cerr<<"Parallel periodic insertion sort done ("<<pass+1<<") passes, "<<chunks<<" chunks; inversions remaining: "<<countInversions().transpose()<<endl;
}

void InsertionSortCollider::insertionSortPeri_part(VecBounds& v, bool doCollide, int ax, long iBegin, long iEnd, long iStart){
	// cerr<<"xyz"[ax]<<": iBegin="<<iBegin<<", iStart="<<iStart<<", iEnd="<<iEnd<<", size="<<v.size<<endl;
	assert(periodic);
	assert(v.size>0);
	assert(iBegin<iEnd);
	// iBegin must be in the normalized range
	assert(v.norm(iBegin)==iBegin);
	assert(0<=iEnd);
	assert(iStart<iEnd);
	long &loIdx=v.loIdx;
	assert(loIdx<v.size);
	#ifdef WOO_OPENMP
		const bool earlyStop=(iBegin!=iStart);
		const bool partial=(v.norm(iBegin)!=v.norm(iEnd));
		// cerr<<"[["<<iBegin<<","<<iEnd<<"("<<v.norm(iEnd)<<"),"<<iStart<<"]]";
	#else
		const bool earlyStop=false;
		const bool partial=false;
		// cerr<<"[["<<iBegin<<"("<<v.norm(iBegin)<<"),"<<iEnd<<"("<<v.norm(iEnd)<<"),"<<iStart<<"]]";
		// assert(iBegin==iStart); // this one does not necessarily hold
		assert(v.norm(iBegin)==v.norm(iEnd));
	#endif
	// don't start at iBegin+1 for partial sort, since we may need to adjust loIdx at that index as well
	// for single-threaded sort, we need iBegin, since j may go down arbitrarily (smaller than iBegin)
	for(long _i=max(iStart,iBegin); _i<iEnd; _i++){
		// TODO: check that we threads cannot change loIdx concurrently!!!
		const long i=v.norm(_i);
		const long i_1=v.norm(i-1);
		// switch period of (i) if the coord is below the lower edge cooridnate-wise and just above the split
		// make sure we don't push loIdx up to other parallel thread's range
		if(i==loIdx && v[i].coord<0 && (!partial || i!=iEnd-1)){ v[i].period-=1; v[i].coord+=v.cellDim; loIdx=v.norm(loIdx+1); }
		// coordinate of v[i] used to check inversions
		// if crossing the split, adjust by cellDim;
		// if we get below the loIdx however, the v[i].coord will have been adjusted already, no need to do that here
		const Real iCmpCoord=v[i].coord+(i==loIdx ? v.cellDim : 0); 
		// no inversion, early return
		if(v[i_1].coord<=iCmpCoord){
			if(WOO_UNLIKELY(earlyStop)){ return; }
			continue;
		}
		// vi is the copy that will travel down the list, while other elts go up
		// if will be placed in the list only at the end, to avoid extra copying
		long iter=0;
		int j=i_1; Bounds vi=v[i];  const bool viHasBB=vi.flags.hasBB; const bool viIsMin=vi.flags.isMin; const bool viIsInf=vi.flags.isInf;
		while((!partial || j>=iBegin) && v[j].coord>vi.coord + /* wrap for elt just below split */ (v.norm(j+1)==loIdx ? v.cellDim : 0)){
			j=v.norm(j);
			long j1=v.norm(j+1);
			// OK, now if many bodies move at the same pace through the cell and at one point, there is inversion,
			// this can happen without any side-effects
			#if 0
				if (v[j].coord>2*v.cellDim){
					// this condition is not strictly necessary, but the loop of insertionSort would have to run more times.
					// Since size of particle is required to be < .5*cellDim, this would mean simulation explosion anyway
					LOG_FATAL("Particle #{} going faster than 1 cell in one step? Not handled.",v[j].id);
					throw runtime_error(__FILE__ ": particle moving too fast (skipped 1 cell).");
				}
			#endif

			Bounds& vNew(v[j1]); // elt at j+1 being overwritten by the one at j and adjusted
			vNew=v[j];
			// inversions close the the split need special care
			// parallel: loIdx modification is safe since j>iBegin (when loIdx is incremented) and j1>iBegin+1 (when loIdx is decremented);
			if(WOO_UNLIKELY(j==loIdx && vi.coord<0)) { vi.period-=1; vi.coord+=v.cellDim; loIdx=v.norm(loIdx+1); }
			else if(WOO_UNLIKELY(j1==loIdx)) { vNew.period+=1; vNew.coord-=v.cellDim; loIdx=v.norm(loIdx-1); }
			if(viIsMin!=v[j].flags.isMin && WOO_LIKELY(doCollide && viHasBB && v[j].flags.hasBB)){
				// see https://bugs.launchpad.net/woo/+bug/669095 and similar problem in aperiodic insertionSort
				if(WOO_LIKELY(vi.id!=vNew.id)){
					handleBoundInversionPeri(vi.id,vNew.id,/*separating*/(!viIsMin && !viIsInf && !v[j].flags.isInf));
				}
			}
			#ifdef WOO_DEBUG
				#ifdef WOO_OPENMP
					#pragma omp critical
				#endif
				{ stepInvs[ax]++; numInvs[ax]++; }
			#endif
			j=v.norm(j-1); // should allow j==-1 so that j<=iBegin works in the conditional ??!
			#if 1
				static int nWarn=0;
				const int maxWarn=50;
				if((iter++==80000 || iter>100000) && nWarn<maxWarn){
					LOG_ERROR("iter={}, partial={}, iBegin={}, i={}, i_1={}, j={}, vi.coord={}, v[v.norm(j)].coord={}, v.norm(j+1)={}, loIdx={}, v.cellDim={}",iter,partial,iBegin,i,i_1,j,vi.coord,v[v.norm(j)].coord,v.norm(j+1),loIdx,v.cellDim);
					nWarn++;
					if(nWarn==maxWarn) LOG_ERROR("[Further errors from insertion sort will be supressed]");
				}
			#endif
		}
		v[v.norm(j+1)]=vi;
	}
}

#else

void InsertionSortCollider::insertionSortPeri_part(VecBounds& v, bool doCollide, int ax, long iBegin, long iEnd, long iStart){ throw std::runtime_error("Not active with old insertSortPeri"); }
void InsertionSortCollider::insertionSortPeri(VecBounds& v, bool doCollide, int ax){
	assert(periodic);
	long &loIdx=v.loIdx; const long &size=v.size;
	for(long _i=0; _i<size; _i++){
		const long i=v.norm(_i);
		const long i_1=v.norm(i-1);
		//switch period of (i) if the coord is below the lower edge cooridnate-wise and just above the split
		if(i==loIdx && v[i].coord<0){ v[i].period-=1; v[i].coord+=v.cellDim; loIdx=v.norm(loIdx+1); }
		// else if(i==v.norm(loIdx+1) && v[i].coord>v.cellDim){ v[i].period+=1; v[i].coord-=v.cellDim; loIdx=v.norm(loIdx-1); }
		// coordinate of v[i] used to check inversions
		// if crossing the split, adjust by cellDim;
		// if we get below the loIdx however, the v[i].coord will have been adjusted already, no need to do that here
		const Real iCmpCoord=v[i].coord+(i==loIdx ? v.cellDim : 0); 
		// no inversion
		if(v[i_1].coord<=iCmpCoord) continue;
		// vi is the copy that will travel down the list, while other elts go up
		// if will be placed in the list only at the end, to avoid extra copying
		int j=i_1; Bounds vi=v[i];  const bool viHasBB=vi.flags.hasBB; const bool viIsMin=vi.flags.isMin; const bool viIsInf=vi.flags.isInf;
		while(v[j].coord>vi.coord + /* wrap for elt just below split */ (v.norm(j+1)==loIdx ? v.cellDim : 0)){
			long j1=v.norm(j+1);
			// OK, now if many bodies move at the same pace through the cell and at one point, there is inversion,
			// this can happen without any side-effects
			Bounds& vNew(v[j1]); // elt at j+1 being overwritten by the one at j and adjusted
			vNew=v[j];
			// inversions close the the split need special care
			if(WOO_UNLIKELY(j==loIdx && vi.coord<0)) { vi.period-=1; vi.coord+=v.cellDim; loIdx=v.norm(loIdx+1); }
			else if(WOO_UNLIKELY(j1==loIdx)) { vNew.period+=1; vNew.coord-=v.cellDim; loIdx=v.norm(loIdx-1); }
			if(viIsMin!=v[j].flags.isMin && WOO_LIKELY(doCollide && viHasBB && v[j].flags.hasBB)){
				// see https://bugs.launchpad.net/woo/+bug/669095 and similar problem in aperiodic insertionSort
				if(WOO_LIKELY(vi.id!=vNew.id)){
					handleBoundInversionPeri(vi.id,vNew.id,/*separating*/(!viIsMin && !viIsInf && !v[j].flags.isInf));
				}
			}
			#ifdef WOO_DEBUG
				stepInvs[ax]++; numInvs[ax]++;
			#endif
			j=v.norm(j-1);
		}
		v[v.norm(j+1)]=vi;
	}
}
#endif

// called by the insertion sort if 2 bodies swapped their bounds
void InsertionSortCollider::handleBoundInversionPeri(Particle::id_t id1, Particle::id_t id2, bool separating){
	assert(periodic);
	// do bboxes overlap in all 3 dimensions?
	Vector3i periods;
	bool overlap=separating?false:spatialOverlapPeri(id1,id2,scene,periods);
	// existing interaction?
	const shared_ptr<Contact>& C=dem->contacts->find(id1,id2);
	bool hasCon=(bool)C;
	#ifdef PISC_DEBUG
		if(watchIds(id1,id2)) LOG_DEBUG("Inversion #{}+#{}, overlap=={}, hasCon=={}",id1,id2,overlap,hasCon);
	#endif
	// interaction doesn't exist and shouldn't, or it exists and should
	if(WOO_LIKELY(!overlap && !hasCon)) return;
	if(overlap && hasCon){  return; }
	// create interaction if not yet existing
	if(overlap && !hasCon){ // second condition only for readability
		#ifdef PISC_DEBUG
			if(watchIds(id1,id2)) LOG_DEBUG("Attemtping collision of #{}+#{}",id1,id2);
		#endif
		const shared_ptr<Particle>& pA((*particles)[id1]);
		const shared_ptr<Particle>& pB((*particles)[id2]);
		if(!Collider::mayCollide(dem,pA,pB)) return;
		makeContactLater(pA,pB,periods);
		#ifdef PISC_DEBUG
			if(watchIds(id1,id2)) LOG_DEBUG("Created intr #{}+#{}, periods={}",id1,id2,periods);
		#endif
		return;
	}
	if(!overlap && hasCon){
		if(!C->isReal()) {
			removeContactLater(C);
			#ifdef PISC_DEBUG
				if(watchIds(id1,id2)) LOG_DEBUG("Erased intr #{}+#{}",id1,id2);
			#endif
		}
		return;
	}
	assert(false); // unreachable
}

py::object InsertionSortCollider::pySpatialOverlap(const shared_ptr<Scene>& S, Particle::id_t id1, Particle::id_t id2){
	if(S.get()!=scene) throw std::runtime_error("Scene object is not the same as this engine was used with.");
	if(min(id1,id2)<0 || max(id1,id2)>(Particle::id_t)particles->size()) throw std::runtime_error("Particle ids outisde of valid range (0.."+to_string(particles->size()));
	if(scene->isPeriodic){
		Vector3i periods;
		bool over=spatialOverlapPeri(id1,id2,scene,periods);
		return py::make_tuple(over,periods);
	} else {
		return py::cast(spatialOverlap(id1,id2));
	}
}


bool InsertionSortCollider::spatialOverlapPeri_axis(const int& axis, const Particle::id_t& id1, const Particle::id_t& id2, const Real& min1, const Real& max1, const Real& min2, const Real& max2, const Real& dim, int& period) const {
	//__attribute__((unused)) const Real &mn1(minima[3*id1+axis]), &mx1(maxima[3*id1+axis]), &mn2(minima[3*id2+axis]), &mx2(maxima[3*id2+axis]);
	assert(!isnan(min1)); assert(!isnan(max1));
	assert(!isnan(min2)); assert(!isnan(max2));
	assert(isinf(max1) || (max1-min1<dim)); assert(isinf(max2) || (max2-min2<dim));
	// infinite particles always overlap, cut it short
	if(isinf(min1) || isinf(min2)){
		assert(!isinf(min1) || min1<0); // check that minimum is minus infinity
		assert(!isinf(min2) || min2<0); // check that minimum is minus infinity
		period=0; // does not matter where the contact happens really
		#ifdef PISC_DEBUG
			if(watchIds(id1,id2)){ LOG_DEBUG("    Some particle infinite along the {} axis.",axis); }
		#endif
		return true; // do other axes
	}
	// compare old and new algorithms
	#ifdef PISC_DBG_NEW_PERIOD_ALGO
		int origPeriod; bool origResult; bool origFailed=false;
	#endif
	int newPeriod; bool newResult;

	// original algorithm
	if(periDbgNew){
		// find particle of which minimum when taken as period start will make the gap smaller
		/*
	
			<.........................> dim
	      <----------->               cellWrapRel(min2,min1,min1+dim)
	      <--------------------->     cellWrapRel(max2,min1,min1+dim)
		   |------------------|        min1..max1
		 	             |--------|     min2..max2
	
	
			<.........................> dim
	      <------------->             cellWrapRel(min2,min2,min2+dim)
	      <---->                      cellWrapRel(max1,min2,min2+dim)
		   ------|        |----------- min1..max1
		 	|--------|                  min2..max2
		   
		*/
		const Real& wMn=(cellWrapRel(min1,min2,min2+dim)<cellWrapRel(min2,min1,min1+dim)) ? min2 : min1;
		#ifdef PISC_DEBUG
			if(watchIds(id1,id2)){
				TRVAR4(id1,id2,axis,dim);
				TRVAR4(min1,max1,min2,max2);
				TRVAR2(cellWrapRel(min1,min2,min2+dim),cellWrapRel(min2,min1,min1+dim));
				TRVAR3(min1,min2,wMn);
			}
		#endif
		int pmn1,pmx1,pmn2,pmx2;
		Real mn1=cellWrap(min1,wMn,wMn+dim,pmn1), mx1=cellWrap(max1,wMn,wMn+dim,pmx1);
		Real mn2=cellWrap(min2,wMn,wMn+dim,pmn2), mx2=cellWrap(max2,wMn,wMn+dim,pmx2);
		if(WOO_UNLIKELY((pmn1!=pmx1) || (pmn2!=pmx2))){
			Real span=(pmn1!=pmx1?mx1-mn1:mx2-mn2); if(span<0) span=dim-span;
			LOG_INFO("Particle #{} spans over half of the cell size {} (axis={}, min={}, max={}, span={})",(pmn1!=pmx1?id1:id2),dim,axis,(pmn1!=pmx1?mn1:mn2),(pmn1!=pmx1?mx1:mx2),span);
			LOG_INFO("Does not matter, try with the new algo now :)");
			#ifdef PISC_DBG_NEW_PERIOD_ALGO
				origFailed=true;
			#endif
			// throw runtime_error("InsertionSortCollider: (old algo limitation) Particle larger than half of the cell size encountered.");
		}
		#ifdef PISC_DBG_NEW_PERIOD_ALGO
			origPeriod=(int)(pmn1-pmn2);
			origResult=(mn1<=mx2 && mx1>=mn2); //) origResult=false;
			// else origResult=true;
			#ifdef PISC_DEBUG
				if(watchIds(id1,id2)){
					TRVAR4(mn1,mx1,mn2,mx2);
					TRVAR4(pmn1,pmx1,pmn2,pmx2);
					TRVAR2(origPeriod,origResult);
				}
			#endif
		#endif
	}

	// new algorithm
	{
		int pmn1_2,pmx1_2,pmn2_1,pmx2_1;
		// minimum and maximum relative to the other particle's minimum
		Real mn1_2=cellWrapRel(min1,min2,min2+dim,pmn1_2); (void)cellWrapRel(max1,min2,min2+dim,pmx1_2);
		Real mn2_1=cellWrapRel(min2,min1,min1+dim,pmn2_1); (void)cellWrapRel(max2,min1,min1+dim,pmx2_1);
		// overlap on both sides (either way does not fit in the cell size)
		if(pmn1_2!=pmx1_2 && pmn2_1!=pmx2_1){
			throw std::runtime_error("InsertionSortCollider: ambiguous periodic contact ##"+to_string(id1)+"+"+to_string(id2)+" (axis="+to_string(axis)+", #"+to_string(id1)+" "+to_string(min1)+".."+to_string(max1)+", #"+to_string(id2)+" "+to_string(min2)+".."+to_string(max2)+", cell size "+to_string(dim)+".");
		}
		// this handles *also* the special case when minima are positioned exactly equally in the cell
		// since we allow both pmn1_2==pmx1_2 and pmn1_2!=pmx1_2
		else if(pmn2_1==pmx2_1){
			// original algo (wMn=min1): pmn1=0, pmx1=0, pmn2=pmn2_1, pmx2=pmx2_1
			newPeriod=-pmn2_1;
			newResult=(mn2_1<(max1-min1));
		} else {
			assert(pmn2_1!=pmx2_1); assert(pmn1_2==pmx1_2);
			// original algo (wMn=min2): pmn1=pmn1_2, pmx1=pmx1_2, pmn2=0, pmx2=0
			newPeriod=pmn1_2;
			newResult=(mn1_2<(max2-min2));
		}
		#ifdef PISC_DEBUG
			if(watchIds(id1,id2)){
				TRVAR2(mn1_2,max1-min1);
				TRVAR2(mn2_1,max2-min2);
				TRVAR4(pmn1_2,pmx1_2,pmn2_1,pmx2_1);
				TRVAR2(newPeriod,newResult);
			}
		#endif
	}
	// Real mn2=cellWrap(min2,wMn,wMn+dim,pmn2), mx2=cellWrap(max2,wMn,wMn+dim,pmx2);
	//period=(int)(pmn1-pmn2);
	//if(!(mn1<=mx2 && mx1 >= mn2)) return false;
	//return true;

	#ifdef PISC_DBG_NEW_PERIOD_ALGO
		// compare old and new algos; period difference is only significant if there was overlap found
		if(periDbgNew && !origFailed && ((origResult && origPeriod!=newPeriod) || origResult!=newResult)){
			if(origPeriod!=newPeriod) LOG_FATAL("Period mismatch in ##{}+{}: old={}, new={}",id1,id2,origPeriod,newPeriod);
			if(origResult!=newResult) LOG_FATAL("Overlap mispatch in ##{}+{}: old={}, new={}",id1,id2,origResult,newResult);
			LOG_FATAL("#{}: axis {}, span {}..{}",id1,axis,min1,max1);
			LOG_FATAL("#{}: axis {}, span {}..{}",id2,axis,min2,max2);
		}
	#endif
	period=newPeriod;
	return newResult;
}

/* Performance hint
	================

	Since this function is called (most the time) from insertionSort,
	we actually already know what is the overlap status in that one dimension, including
	periodicity information; both values could be passed down as a parameters, avoiding 1 of 3 loops here.
	We do some floats math here, so the speedup could noticeable; doesn't concertn the non-periodic variant,
	where it is only plain comparisons taking place.

	In the same way, handleBoundInversion is passed only id1 and id2, but again insertionSort already knows in which sense
	the inversion happens; if the boundaries get apart (min getting up over max), it wouldn't have to check for overlap
	at all, for instance.
*/
//! return true if bodies bb overlap in all 3 dimensions
bool InsertionSortCollider::spatialOverlapPeri(Particle::id_t id1, Particle::id_t id2, Scene* scene, Vector3i& periods) const {
	assert(periodic);
	assert(id1!=id2); // programming error, or weird bodies (too large?)
	#if 0
		/*
		TODO: check performance impact when this type of logic is not needed
		even though mask check is present in Collider::mayCollide, spatialOverlapPeri_axis
		will throw exception for ambiguous contacts.
		*/
		if((*particles)[id1]->mask & (*particles)[id2]->mask & dem->loneMask) return false;
	#endif
	for(int axis=0; axis<3; axis++){
		if(!spatialOverlapPeri_axis(axis,id1,id2,minima[3*id1+axis],maxima[3*id1+axis],minima[3*id2+axis],maxima[3*id2+axis],scene->cell->getSize()[axis],periods[axis])) return false;
	}
	#ifdef PISC_DEBUG
		if(watchIds(id1,id2)) LOG_DEBUG("Overlap #{}+#{}, periods {}",id1,id2,periods);
	#endif
	return true;
}

py::tuple InsertionSortCollider::dumpBounds(){
	py::list bl[3]; // 3 bound lists, inserted into the tuple at the end
	for(int axis=0; axis<3; axis++){
		VecBounds& V=BB[axis];
		if(periodic){
			for(long i=0; i<V.size; i++){
				long ii=V.norm(V.loIdx+i); // start from the period boundary
				vector<string> flg;
				if(!V[ii].flags.hasBB) flg.push_back("nobb");
				if(V[ii].flags.isInf) flg.push_back("inf");
				if(ii==V.loIdx) flg.push_back("||");
				bl[axis].append(py::make_tuple(V[ii].coord,(V[ii].flags.isMin?-1:1)*V[ii].id,V[ii].period,boost::algorithm::join(flg," ")));
			}
		} else {
			for(long i=0; i<V.size; i++){
				bl[axis].append(py::make_tuple(V[i].coord,(V[i].flags.isMin?-1:1)*V[i].id));
			}
		}
	}
	return py::make_tuple(bl[0],bl[1],bl[2]);
}

py::object InsertionSortCollider::dbgInfo(){
	py::dict ret;
	return ret;
}

#ifdef NO_ABSTRACT_AABB_COLLIDER
	void InsertionSortCollider::pyHandleCustomCtorArgs(py::args_& t, py::kwargs& d){
		if(py::len(t)==0) return; // nothing to do
		if(py::len(t)!=1) throw invalid_argument(("Collider optionally takes exactly one list of BoundFunctor's as non-keyword argument for constructor ("+to_string(py::len(t))+" non-keyword ards given instead)").c_str());
		if(py::len(t)!=1) throw invalid_argument("GridCollider optionally takes exactly one list of GridBoundFunctor's as non-keyword argument for constructor ("+to_string(py::len(t))+" non-keyword ards given instead)");
		if(!boundDispatcher) boundDispatcher=make_shared<BoundDispatcher>();
		vector<shared_ptr<BoundFunctor>> vf=py::extract<vector<shared_ptr<BoundFunctor>>>((t[0]))();
		for(const auto& f: vf) boundDispatcher->add(f);
		t=py::tuple(); // empty the args
	}

	void InsertionSortCollider::getLabeledObjects(const shared_ptr<LabelMapper>& labelMapper){ if(boundDispatcher) boundDispatcher->getLabeledObjects(labelMapper); Engine::getLabeledObjects(labelMapper); }
#endif
