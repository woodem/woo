#pragma once
#include<woo/lib/base/Types.hpp>
#include<woo/lib/base/openmp-accu.hpp>
#include<woo/lib/object/Object.hpp>
#include<woo/lib/pyutil/except.hpp>


// namespace py=boost::python;

struct EnergyTrackerGrid: public Object{
	WOO_DECL_LOGGER; 
	typedef boost::multi_array<Real,5> boost_multi_array_real_5;
	void add(const Real& val, int& id, const Vector3r& xyz);
	void init(const AlignedBox3r& _box, const Real& _cellSize, int maxIndex);
	inline Vector3i xyz2ijk(const Vector3r& xyz) const { Vector3r t=(xyz-box.min())/cellSize; return Vector3i(floor(t[0]),floor(t[1]),floor(t[2])); }
	inline Vector3r ijk2xyz(const Vector3i& ijk) const { return box.min()+ijk.cast<Real>()*cellSize; }
	void vtkExport(const string& out, const vector<string>& names);
	#define woo_core_EnergyTrackerGrid__CLASS_BASE_DOC_ATTRS_PY \
		EnergyTrackerGrid,Object,"Storage of spatially located energy values.", \
		((AlignedBox3r,box,,AttrTrait<Attr::readonly>(),"Part of space which we monitor."))\
		((Real,cellSize,NaN,AttrTrait<Attr::readonly>(),"Size of one cell in the box (in all directions); will be satisfied exactly, at the expense of slightly growing :obj:`box`. Do not change.")) \
		((Vector3i,boxCells,Vector3i::Zero(),AttrTrait<Attr::readonly>(),"Number of cells in the box (computed automatically).")) \
		((boost_multi_array_real_5,data,boost_multi_array_real_5(boost::extents[0][0][0][0][0]),AttrTrait<Attr::hidden>(),"Grid data -- 5d since each 3d point contains multiple energies, and there are multiple threads writing concurrently.")) \
		,/*py*/ .def("vtkExport",&EnergyTrackerGrid::vtkExport,WOO_PY_ARGS(py::arg("out"),py::arg("names")),"Export data into VTK grid file *out*, using *names* to name the arrays exported.")

	WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_core_EnergyTrackerGrid__CLASS_BASE_DOC_ATTRS_PY);
};
WOO_REGISTER_OBJECT(EnergyTrackerGrid);

class EnergyTracker: public Object{
	WOO_DECL_LOGGER; 
	public:
	/* in flg, IsIncrement is a pseudo-value which doesn't do anything; is meant to increase readability of calls */
	enum{ IsIncrement=0, IsResettable=1, ZeroDontCreate=2 };
	void findId(const std::string& name, int& id, int flg, bool newIfNotFound=true){
		if(id>0) return; // the caller should have checked this already
		if(names.count(name)) id=names[name];
		else if(newIfNotFound) {
			#ifdef WOO_OPENMP
				#pragma omp critical
			#endif
				{ energies.resize(energies.size()+1); id=energies.size()-1; flags.resize(id+1); flags[id]=flg; names[name]=id; assert(id<(int)energies.size()); assert(id>=0); }
		}
	}
	// set value of the accumulator; note: must NOT be called from parallel sections!
	void set(const Real& val, const std::string& name, int &id){
		if(id<0) findId(name,id,/* do not reset value that is set directly, although it is not "incremental" */ IsIncrement);
		energies.set(id,val);
	}
	// add value to the accumulator; safely called from parallel sections
	void add(const Real& val, const std::string& name, int &id, int flg, const Vector3r& xyz=Vector3r(NaN,NaN,NaN));
	// add value from python (without the possibility of caching index, do name lookup every time)
	void add_py(const Real& val, const std::string& name, bool reset=false);

	py::dict names_py() const;
	Real getItem_py(const std::string& name);
	void setItem_py(const std::string& name, Real val);
	bool contains_py(const std::string& name);
	int len_py() const;
	void clear();
	void resetResettables(){ size_t sz=energies.size(); for(size_t id=0; id<sz; id++){ if(flags[id] & IsResettable) energies.reset(id); } }

	Real total() const;
	Real relErr() const;
	py::list keys_py() const;
	py::list items_py() const;
	py::dict perThreadData() const;

	typedef std::map<std::string,int> mapStringInt;
	typedef std::pair<std::string,int> pairStringInt;

	class pyIterator{
		const shared_ptr<EnergyTracker> et; mapStringInt::iterator I;
	public:
		pyIterator(const shared_ptr<EnergyTracker>&);
		pyIterator iter();
		string next();
	};
	pyIterator pyIter();
	// grid manipulation
	void gridOn(const AlignedBox3r& box, Real cellSize, int maxIndex=-1);
	void gridOff();
	string gridToVTK(const string& out);

	#define woo_core_EnergyTracker__CLASS_BASE_DOC_ATTRS_PY \
		EnergyTracker,Object,"Storage for tracing energies. Only to be used if O.traceEnergy is True.", \
		((OpenMPArrayAccumulator<Real>,energies,,,"Energy values, in linear array")) \
		((mapStringInt,names,,/*no python converter for this type*/AttrTrait<Attr::hidden>(),"Associate textual name to an index in the energies array [overridden bellow]."))  \
		((shared_ptr<EnergyTrackerGrid>,grid,,AttrTrait<Attr::readonly>(),"Grid for tracking spatial distribution of energy increments; write-protected, use :obj:`gridOn`, :obj:`gridOff`.")) \
		((vector<int>,flags,,AttrTrait<Attr::readonly>(),"Flags for respective energies; most importantly, whether the value should be reset at every step.")) \
		, /*py*/ \
			.def("__len__",&EnergyTracker::len_py,"Number of items in the container.") \
			.def("__getitem__",&EnergyTracker::getItem_py,"Get energy value for given name.") \
			.def("__setitem__",&EnergyTracker::setItem_py,"Set energy value for given name (will create a non-resettable item, if it does not exist yet).") \
			.def("__contains__",&EnergyTracker::contains_py,"Query whether given key exists; used by ``'key' in EnergyTracker``") \
			.def("__iter__",&EnergyTracker::pyIter,"Return iterator over keys, to support python iteration protocol.") \
			.def("add",&EnergyTracker::add_py,WOO_PY_ARGS(py::arg("dE"),py::arg("name"),py::arg("reset")=false),"Accumulate energy, used from python (likely inefficient)") \
			.def("clear",&EnergyTracker::clear,"Clear all stored values.") \
			.def("keys",&EnergyTracker::keys_py,"Return defined energies.") \
			.def("items",&EnergyTracker::items_py,"Return contents as list of (name,value) tuples.") \
			.def("total",&EnergyTracker::total,"Return sum of all energies.") \
			.def("relErr",&EnergyTracker::relErr,"Total energy divided by sum of absolute values.") \
			.add_property_readonly("names",&EnergyTracker::names_py) /* return name->id map as python dict */ \
			.add_property_readonly("_perThreadData",&EnergyTracker::perThreadData,"Contents as dictionary, where each value is tuple of individual threads' values (for debugging)") \
			.def("gridOn",&EnergyTracker::gridOn,WOO_PY_ARGS(py::arg("box"),py::arg("cellSize"),py::arg("maxIndex")=-1),"Initialize :obj:`grid` object, which will record spacial location of energy events.") \
			.def("gridOff",&EnergyTracker::gridOff,"Disable :obj:`grid` so that energy location is not recorded anymore. The :obj:`grid` object is discarded, including any data it might have contained.") \
			.def("gridToVTK",&EnergyTracker::gridToVTK,WOO_PY_ARGS(py::arg("out")),"Write grid data to VTK file *out* (``.vti`` will be appended); returns output file name.") \
			; /* define nested class */ \
			py::class_<EnergyTracker::pyIterator>(mod,"EnergyTracker_iterator").def("__iter__",&EnergyTracker::pyIterator::iter).def("__next__",&pyIterator::next);
	WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_core_EnergyTracker__CLASS_BASE_DOC_ATTRS_PY);
};
WOO_REGISTER_OBJECT(EnergyTracker);
