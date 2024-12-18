// © 2009 Václav Šmilauer <eudoxos@arcig.cz>

#pragma once

#include<vector>
#include<string>
#include<limits>

#include"../base/Types.hpp"
#include"../base/Logging.hpp"
#include"../base/Math.hpp"

#include"../pyutil/except.hpp"

/* base class for filtering predicates */
struct Predicate{
	WOO_DECL_LOGGER;
	public:
		virtual bool operator() (const Vector3r& pt,Real pad=0.) const = 0;
		virtual AlignedBox3r aabb() const = 0;
		virtual ~Predicate()=default;
		Vector3r dim() const { return aabb().sizes(); }
		Vector3r center() const { return aabb().center(); }
};



/*! Class representing geometry of spherical packing, with some utility functions. */
class SpherePack{
	struct ClumpInfo{ int clumpId; Vector3r center; Real rad; int minId, maxId; };
public:
	// return coordinate wrapped to x0…x1, relative to x0; don't care about period
	// copied from PeriodicInsertionSortCollider
	static Real cellWrapRel(const Real x, const Real x0, const Real x1){
		Real xNorm=(x-x0)/(x1-x0);
		return (xNorm-floor(xNorm))*(x1-x0);
	}
	Real periPtDistSq(const Vector3r& p1, const Vector3r& p2){
		Vector3r dr;
		for(int ax: {0,1,2}){
			if(cellSize[ax]>0) dr[ax]=min(cellWrapRel(p1[ax],p2[ax],p2[ax]+cellSize[ax]),cellWrapRel(p2[ax],p1[ax],p1[ax]+cellSize[ax]));
			else dr[ax]=p2[ax]-p1[ax]; // don't care about sign
		}
		return dr.squaredNorm();
	}
	enum {RDIST_RMEAN, RDIST_NUM, RDIST_PSD};
	struct Sph{
		Vector3r c; Real r; int clumpId; int shadowOf;
		Sph(const Vector3r& _c, Real _r, int _clumpId=-1, int _shadowOf=-1): c(_c), r(_r), clumpId(_clumpId), shadowOf(_shadowOf) {};
		py::tuple asTuple() const {
			if(clumpId<0) return py::make_tuple(c,r);
			return py::make_tuple(c,r,clumpId);
		}
		py::tuple asTupleNoClump() const { return py::make_tuple(c,r); }
	};
	std::vector<Sph> pack;
	Vector3r cellSize;
	string userData;
	Real psdScaleExponent;
	Real appliedPsdScaling;//a scaling factor that can be applied on size distribution
	SpherePack(): cellSize(Vector3r::Zero()), psdScaleExponent(2.5), appliedPsdScaling(1.){};
	SpherePack(const py::list& l):cellSize(Vector3r::Zero()){ fromList(l); }
	void reset(){ pack.clear(); cellSize=Vector3r::Zero(); userData.clear(); }
	// add single sphere
	void add(const Vector3r& c, Real r, int clumpId=-1){ pack.push_back(Sph(c,r,clumpId)); }

	// I/O
	void fromList(const py::list& l);
	void fromLists(const vector<Vector3r>& centers, const vector<Real>& radii); // used as ctor in python
	py::list toList() const;
	py::tuple toCcRr() const;
	void fromFile(const string& file);
	void toFile(const string& file) const;
	// void fromSimulation();

	shared_ptr<SpherePack> filtered(const shared_ptr<Predicate>& p, bool recenter=true);


	// random generation; if num<0, insert as many spheres as possible; if porosity>0, recompute meanRadius (porosity>0.65 recommended) and try generating this porosity with num spheres.
	long makeCloud(Vector3r min, Vector3r max, Real rMean=-1, Real rFuzz=0, int num=-1, bool periodic=false, Real porosity=-1, const vector<Real>& psdSizes=vector<Real>(), const vector<Real>& psdCumm=vector<Real>(), bool distributeMass=false, int seed=0, Matrix3r hSize=Matrix3r::Zero());

	// for wrapping in py3k which needs simpler args for reasons unknown
	long makeCloud_simple(const Vector3r& mn, const Vector3r& mx, const Real& rMean, const Real& rRelFuzz, int num, bool periodic);

	// return number of piece for x in piecewise function defined by cumm with non-decreasing elements ∈(0,1)
	// norm holds normalized coordinate withing the piece
	int psdGetPiece(Real x, const vector<Real>& cumm, Real& norm);


	// interpolate a variable with power distribution (exponent -3) between two margin values, given uniformly distributed x∈(0,1)
	Real pow3Interp(Real x,Real a,Real b){ return pow(x*(pow(b,-2)-pow(a,-2))+pow(a,-2),-1./2); }


	#if 0
		// generate packing of clumps, selected with equal probability
		// periodic boundary is supported
		long makeClumpCloud(const Vector3r& mn, const Vector3r& mx, const vector<shared_ptr<SpherePack> >& clumps, bool periodic=false, int num=-1);
	#endif

	// add/remove shadow spheres with periodic boundary, for spheres crossing it
	int addShadows();
	int removeShadows();

	// periodic repetition
	void cellRepeat(Vector3i count);
	void cellFill(Vector3r volume);
	void canonicalize();

	// spatial characteristics
	Vector3r dim() const { return aabb().sizes();}
	AlignedBox3r aabb() const {
		AlignedBox3r ret;
		for(const Sph& s: pack){ ret.extend(s.c+Vector3r::Constant(s.r)); ret.extend(s.c-Vector3r::Constant(s.r)); }
		return ret;
	}
	Vector3r midPt() const { return aabb().center();}
	Real relDensity() const { return  sphereVol()/dim().prod(); }
	Real sphereVol() const;

	py::tuple psd(int bins=10, bool mass=false) const;
	bool hasClumps() const;
	py::tuple getClumps() const;

	// transformations
	void translate(const Vector3r& shift){ for(Sph& s: pack) s.c+=shift; }
	void rotate(const Vector3r& axis, Real angle){
		if(cellSize!=Vector3r::Zero()) { LOG_WARN("Periodicity reset when rotating periodic packing (non-zero cellSize={})",cellSize); cellSize=Vector3r::Zero(); }
		Vector3r mid=midPt(); Quaternionr q(AngleAxisr(angle,axis)); for(Sph& s: pack) s.c=q*(s.c-mid)+mid;
	}
	void rotateAroundOrigin(const Quaternionr& rot){
		if(cellSize!=Vector3r::Zero()){ LOG_WARN("Periodicity reset when rotating periodic packing (non-zero cellSize={})",cellSize); cellSize=Vector3r::Zero(); }
		for(Sph& s: pack) s.c=rot*s.c;
	}
	void scale(Real scale, bool keepRadius=false);
	Real maxRelOverlap();
	void makeOverlapFree(){ scale(maxRelOverlap()+1,/*keepRadius*/true); }
	int pruneOverlapping(float minRelOverlap);


	// iteration 
	size_t len() const{ return pack.size(); }
	py::tuple getitem(size_t idx){ if(/*idx<0||*/idx>=pack.size()) throw runtime_error("Index "+to_string(idx)+" out of range 0.."+to_string(pack.size()-1)); return pack[idx].asTuple(); }
	struct _iterator{
		const SpherePack& sPack; size_t pos;
		_iterator(const SpherePack& _sPack): sPack(_sPack), pos(0){}
		_iterator iter(){ return *this;}
		py::tuple next(){
			if(pos==sPack.pack.size()){ woo::StopIteration(); }
			return sPack.pack[pos++].asTupleNoClump();
		}
	};
	SpherePack::_iterator getIterator() const{ return SpherePack::_iterator(*this);};
	WOO_DECL_LOGGER;
};

