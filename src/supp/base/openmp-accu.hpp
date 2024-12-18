#pragma once 

// for ZeroInitializer template
#include"Math.hpp"
#include"Types.hpp"
#include<string>

#ifdef WOO_OPENMP
#include"omp.h"
#include<cstdlib>
#include<unistd.h>
#include<vector>
#include<stdexcept>
#include<iostream>

// Storage of data is aligned to cache line size, no false sharing should occur (but some space is wasted, OTOH)
// This will currently not compile for non-POSIX systems, as we use sysconf and posix_memalign.

// assume 64 bytes (arbitrary, but safe) if sysconf does not report anything meaningful
// that might happen on newer proc models not yet reported by sysconf (?)
// e.g. https://lists.launchpad.net/woo-dev/msg06294.html
// where zero was reported, leading to FPU exception at startup

#ifdef _SC_LEVEL1_DCACHE_LINESIZE
	// Linux
	#define _WOO_L1_CACHE_LINESIZE (sysconf(_SC_LEVEL1_DCACHE_LINESIZE)>0 ? sysconf(_SC_LEVEL1_DCACHE_LINESIZE) : 64)
#else
	// Windows
	#define _WOO_L1_CACHE_LINESIZE 64
#endif

// trace macro for windows - unable to compile woo with -g
// due to "too many sections" problem (PE32 vs. boost::serialization templates)
#define _MPAA_DBG(a)
// #define _MPAA_DBG(a) cerr<<a;

// O(1) access container which stores data in contiguous chunks of memory
// each chunk belonging to one thread
template<typename T>
class OpenMPArrayAccumulator{
	const int CLS;
	const size_t nThreads;
	const int perCL; // number of elements fitting inside cache line
	std::vector<T*> chunks; // array with pointers to the chunks of memory we have allocated; each item for one thread
	size_t sz; // current number of elements
	size_t nCL; // current number of allocated cache lines
	int nCL_for_N(size_t n){ return n/perCL+(n%perCL==0 ? 0 : 1); } // return number of cache lines to allocate for given number of elements
	public:
		OpenMPArrayAccumulator()        : CLS(_WOO_L1_CACHE_LINESIZE), nThreads(omp_get_max_threads()), perCL(CLS/sizeof(T)), chunks(nThreads,NULL), sz(0), nCL(0) { }
		OpenMPArrayAccumulator(const OpenMPArrayAccumulator& other): OpenMPArrayAccumulator() {
			this->operator=(other);
			//this->resize(a.sz);
			//for(size_t th=0; th<nThreads; th++) memcpy((void*)(chunks[th]),(void*)(a.chunks[th]),nCL_for_N(sz)*CLS);
		}
		OpenMPArrayAccumulator(size_t n): CLS(_WOO_L1_CACHE_LINESIZE), nThreads(omp_get_max_threads()), perCL(CLS/sizeof(T)), chunks(nThreads,NULL), sz(0), nCL(0) { resize(n); }
		void operator=(const OpenMPArrayAccumulator& other){
			this->resize(other.sz);
			assert(this->CLS==other.CLS);
			assert(this->nThreads==other.nThreads);
			assert(this->perCL==other.perCL);
			for(size_t th=0; th<nThreads; th++) memcpy((void*)(chunks[th]),(void*)(other.chunks[th]),nCL_for_N(sz)*CLS);
		}
		// change number of elements
		void resize(size_t n){
			if(n==sz) return; // nothing to do
			size_t nCL_new=nCL_for_N(n);
			_MPAA_DBG("OpenMPArrayAccumulator: resize "<<sz<<" -> "<<n<<" ("<<nCL<<" -> "<<nCL_new<<" lines per "<<CLS<<"b; "<<sizeof(T)<<"b/item)"<<endl);
			if(nCL_new>nCL){
				for(size_t th=0; th<nThreads; th++){
					void* oldChunk=(void*)chunks[th];
					#ifndef __MINGW64__
						int succ=posix_memalign((void**)(&chunks[th]),/*alignment*/CLS,/*size*/ nCL_new*CLS);
						if(succ!=0)
					#else
						// http://msdn.microsoft.com/en-us/library/8z34s9c6%28v=vs.80%29.aspx
						chunks[th]=(T*)_aligned_malloc(/*size*/nCL_new*CLS,/*alignment*/CLS);
						if(chunks[th]==NULL)
					#endif
						throw std::runtime_error("OpenMPArrayAccumulator: _aligned_malloc/posix_memalign failed to allocate memory.");
					_MPAA_DBG("\tthread "<<th<<": alloc "<<nCL_new*CLS<<"b @ "<<chunks[th]<<endl);
					if(oldChunk){ // initialized to NULL initially, that must not be copied and freed
						_MPAA_DBG("\tthread "<<th<<": memcpy "<<oldChunk<<" -> "<<(void*)chunks[th]<<" ("<<nCL*CLS<<"b)"<<endl);
						memcpy(/*dest*/(void*)chunks[th],/*src*/oldChunk,nCL*CLS); // preserve old data
						_MPAA_DBG("\tthread "<<th<<": free "<<oldChunk<<" ("<<nCL*CLS<<"b)"<<endl);
						#ifndef __MINGW64__
							free(oldChunk); // deallocate old storage
						#else
							_aligned_free(oldChunk); // free is illegal with _aligned_malloc
						#endif
					}
					nCL=nCL_new;
				}
				
			}
			// if nCL_new<nCL, do not deallocate memory
			// if nCL_new==nCL, only update sz
			// reset items that were added
			_MPAA_DBG("\tZero'ing new items: ");
			for(size_t th=0; th<nThreads; th++){
				_MPAA_DBG(" [th "<<th<<":");
				for(size_t s=sz; s<n; s++){
					_MPAA_DBG(" "<<s);
					chunks[th][s]=ZeroInitializer<T>();
				}
				_MPAA_DBG("]");
			}
			_MPAA_DBG(endl);
			sz=n;
		}
		~OpenMPArrayAccumulator(){
			/* deallocate memory */
			for(size_t th=0; th<nThreads; th++){
				if(!chunks[th]) continue;
				#ifndef __MINGW64__
					free((void*)chunks[th]);
				#else
					_aligned_free((void*)chunks[th]);
				#endif
			}
		}
		// clear (deallocates storage)
		void clear() { resize(0); }
		// return number of elements
		size_t size() const { return sz; }
		// get value of one element, by summing contributions of all threads
		T operator[](size_t ix) const { return get(ix); }
		T get(size_t ix) const { T ret(ZeroInitializer<T>()); for(size_t th=0; th<nThreads; th++) ret+=chunks[th][ix]; return ret; }
		// set value of one element; all threads are reset except for the 0th one, which assumes that value
		void set(size_t ix, const T& val){ for(size_t th=0; th<nThreads; th++) chunks[th][ix]=(th==0?val:ZeroInitializer<T>()); }
		// reset one element to ZeroInitializer
		void add(size_t ix, const T& diff){ chunks[omp_get_thread_num()][ix]+=diff; }
		void reset(size_t ix){ set(ix,ZeroInitializer<T>()); }
		void resetAll(){ for(size_t i=0; i<sz; i++) reset(i); }
		// fill all memory with zeros; the caller is responsible for assuring that such value is meaningful when converted to T
		// void memsetZero(){ for(size_t th=0; th<nThreads; th++) memset(&chunks[th],0,CLS*nCL); }
		// get all stored data, organized first by index, then by threads; only used for debugging
		std::vector<std::vector<T> > getPerThreadData() const { std::vector<std::vector<T> > ret; for(size_t ix=0; ix<sz; ix++){ std::vector<T> vi; for(size_t th=0; th<nThreads; th++) vi.push_back(chunks[th][ix]); ret.push_back(vi); } return ret; }
};

/* Class accumulating results of type T in parallel sections. Summary value (over all threads) can be read or reset in non-parallel sections only.

#. update value, useing the += operator.
#. Get value using implicit conversion to T (in non-parallel sections only)
#. Reset value by calling reset() (in non-parallel sections only)

*/
template<typename T>
class OpenMPAccumulator{
		const int CLS; // cache line size
		const int nThreads;
		const int eSize; // size of an element, computed from cache line size and sizeof(T)
		T* data=nullptr; // with T*, the pointer arithmetics has sizeof(T) as unit, so watch out!
	public:
	// initialize storage with _zeroValue, depending on nuber of threads
	OpenMPAccumulator(): CLS(_WOO_L1_CACHE_LINESIZE), nThreads(omp_get_max_threads()), eSize(CLS*(sizeof(T)/CLS+(sizeof(T)%CLS==0 ? 0 :1))), data(NULL) {
		#ifndef __MINGW64__
			void *mem=NULL;
			int succ=posix_memalign(/*where allocated*/&mem,/*alignment*/CLS,/*size*/ nThreads*eSize);
			data=(T*)mem;
			if(succ!=0)
		#else
			// http://msdn.microsoft.com/en-us/library/8z34s9c6%28v=vs.80%29.aspx
			data=(T*)_aligned_malloc(/*size*/nThreads*eSize,/*alignment*/CLS);
			if(data==NULL)
		#endif
				throw std::runtime_error("OpenMPAccumulator: posix_memalign/_aligned_malloc failed to allocate memory.");
		reset();
	}
	OpenMPAccumulator(const OpenMPAccumulator& other): OpenMPAccumulator() { this->operator=(other); }
	void operator=(const OpenMPAccumulator& other){
		assert(other.nThreads==nThreads); assert(other.eSize==eSize);
		memcpy((void*)data,(void*)other.data,nThreads*eSize); // copy data over
	}
	~OpenMPAccumulator() {
		assert(data); // would've failed in the ctor already
		#ifndef __MINGW64__
			free((void*)data);
		#else
			_aligned_free((void*)data);
		#endif
	}
	// lock-free addition
	void operator+=(const T& val){ *((T*)(data+omp_get_thread_num()))+=val; }
	// return summary value; must not be used concurrently
	operator T() const { return get(); }
	// reset to zeroValue; must NOT be used concurrently
	void reset(){ for(int i=0; i<nThreads; i++) *(T*)(data+i)=ZeroInitializer<T>(); }
	// this can be used to get the value from python, something like
	// .def_readonly("myAccu",&OpenMPAccumulator::get,"documentation")
	T get() const { T ret(ZeroInitializer<T>()); for(int i=0; i<nThreads; i++) ret+=*(T*)(data+i); return ret; }
	void set(const T& value){ reset(); /* set value for the 0th thread */ *(T*)(data)=value; }
	// only useful for debugging
	std::vector<T> getPerThreadData() const { std::vector<T> ret; for(int i=0; i<nThreads; i++) ret.push_back(*(T*)(data+i)); return ret; }
};
#else 
template<typename T>
class OpenMPArrayAccumulator{
	std::vector<T> data;
	public:
		OpenMPArrayAccumulator(){}
		OpenMPArrayAccumulator(size_t n){ resize(n); }
		void resize(size_t s){ data.resize(s,ZeroInitializer<T>()); }
		void clear(){ data.clear(); }
		size_t size() const { return data.size(); }
		T operator[](size_t ix) const { return get(ix); }
		T get(size_t ix) const { return data[ix]; }
		void add (size_t ix, const T& diff){ data[ix]+=diff; }
		void set(size_t ix, const T& val){ data[ix]=val; }
		void reset(size_t ix){ data[ix]=ZeroInitializer<T>(); }
		void resetAll(){ for(size_t i=0; i<data.size(); i++) reset(i); }
		std::vector<std::vector<T> > getPerThreadData() const { std::vector<std::vector<T> > ret; for(size_t ix=0; ix<data.size(); ix++){ std::vector<T> vi; vi.push_back(data[ix]); ret.push_back(vi); } return ret; }
};

// single-threaded version of the accumulator above
template<typename T>
class OpenMPAccumulator{
	T data;
public:
	void operator+=(const T& val){ data+=val; }
	operator T() const { return get(); }
	void reset(){ data=ZeroInitializer<T>(); }
	T get() const { return data; }
	void set(const T& val){ data=val; }
	// debugging only
	std::vector<T> getPerThreadData() const { std::vector<T> ret; ret.push_back(data); return ret; }
};
#endif

namespace woo{
	void registerOpenMPAccuClassesInPybind11(py::module&);
}

#ifdef WOO_CEREAL
	#include<cereal/cereal.hpp>
	namespace cereal{
		template<class Archive, class Scalar> void save(Archive &ar, const OpenMPAccumulator<Scalar>& a){ Scalar value=a.get(); ar & value; }
		template<class Archive, class Scalar> void load(Archive &ar,        OpenMPAccumulator<Scalar>& a){ Scalar value; ar & value; a.set(value); }

		template<class Archive, class Scalar> void save(Archive &ar, const OpenMPArrayAccumulator<Scalar>& a){ size_t size=a.size(); ar & cereal::make_nvp("size",size); for(size_t i=0; i<size; i++) { Scalar item(a.get(i)); ar & item; } }
		template<class Archive, class Scalar> void load(Archive &ar,       OpenMPArrayAccumulator<Scalar>& a){ size_t size; ar & cereal::make_nvp("size",size); a.resize(size); for(size_t i=0; i<size; i++){ int item; ar & item; a.set(i,item); } }
	};
#else
	#include<boost/serialization/split_free.hpp>
	#include<boost/serialization/nvp.hpp>
	BOOST_SERIALIZATION_SPLIT_FREE(OpenMPAccumulator<int>);
	BOOST_SERIALIZATION_SPLIT_FREE(OpenMPAccumulator<Real>);
	template<class Archive, class Scalar> void save(Archive &ar, const OpenMPAccumulator<Scalar>& a, unsigned int version){ Scalar value=a.get(); ar & BOOST_SERIALIZATION_NVP(value); }
	template<class Archive, class Scalar> void load(Archive &ar,       OpenMPAccumulator<Scalar>& a, unsigned int version){ Scalar value; ar & BOOST_SERIALIZATION_NVP(value); a.set(value); }

	BOOST_SERIALIZATION_SPLIT_FREE(OpenMPArrayAccumulator<int>);
	BOOST_SERIALIZATION_SPLIT_FREE(OpenMPArrayAccumulator<Real>);
	template<class Archive, class Scalar> void save(Archive &ar, const OpenMPArrayAccumulator<Scalar>& a, unsigned int version){ size_t size=a.size(); ar & BOOST_SERIALIZATION_NVP(size); for(size_t i=0; i<size; i++) { Scalar item(a.get(i)); ar & boost::serialization::make_nvp(("item"+std::to_string(i)).c_str(),item); } }
	template<class Archive, class Scalar> void load(Archive &ar,       OpenMPArrayAccumulator<Scalar>& a, unsigned int version){ size_t size; ar & BOOST_SERIALIZATION_NVP(size); a.resize(size); for(size_t i=0; i<size; i++){ Scalar item; ar & boost::serialization::make_nvp(("item"+std::to_string(i)).c_str(),item); a.set(i,item); } }
#endif
