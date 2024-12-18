#pragma once 

#include"../supp/base/Math.hpp"
#include"../core/Scene.hpp"
#include"Particle.hpp"
#include"Sphere.hpp"
#include"Facet.hpp"
#include"../supp/sphere-pack/SpherePack.hpp"

#include <boost/iterator/zip_iterator.hpp>
#include <boost/range.hpp>

struct DemFuncs{
	WOO_DECL_LOGGER;
	static shared_ptr<DemField> getDemField(const Scene* scene);
	static std::tuple</*stress*/Matrix3r,/*stiffness*/Matrix6r> stressStiffness(const Scene* scene, const DemField* dem, bool skipMultinodal, Real volume);
	static Real unbalancedForce(const Scene* scene, const DemField* dem, bool useMaxForce);
	static shared_ptr<Particle> makeSphere(Real radius, const shared_ptr<Material>& m);
	static vector<Particle::id_t> SpherePack_toSimulation_fast(const shared_ptr<SpherePack>& sp, const Scene* scene, const DemField* dem, const shared_ptr<Material>& mat, int mask=0, Real color=NaN);
	static vector<Vector2r> boxPsd(const Scene* scene, const DemField* dem, const AlignedBox3r& box=AlignedBox3r(Vector3r(NaN,NaN,NaN),Vector3r(NaN,NaN,NaN)), bool mass=false, int num=20, int mask=0, Vector2r dRange=Vector2r::Zero());

	static bool particleStress(const shared_ptr<Particle>& p, Vector3r& normal, Vector3r& shear);

	static Real pWaveDt(const shared_ptr<DemField>& field, bool noClumps=false);
	static Real spherePWaveDt(Real radius, Real density, Real young){ return radius/sqrt(young/density); }
	static Real critDt(const shared_ptr<Scene>& scene, const shared_ptr<DemField>& dem, bool noClumps=false);


	// sum force and torque with regards to point pt over particles with matching mask
	// if *multinodal* is true, get force/troque from contacts of multinodal particles
	// (in addition to their nodal forces/torques);
	// returns number of particles matching the mask
	static size_t reactionInPoint(const Scene* scene, const DemField* dem, int mask, const Vector3r& pt, bool multinodal, Vector3r& force, Vector3r& torque);

	static Real coordNumber(const shared_ptr<DemField>& dem, const shared_ptr<Node>& node, const AlignedBox3r& box, int mask=0, bool skipFree=true);

	// compute porosity of given box (in local coords, if node is given) and return the 
	static Real porosity(const shared_ptr<DemField>& dem, const shared_ptr<Node>& node, const AlignedBox3r& box);


	/* return list of quantile values for contact coordinates in node-local z-coordinates, for contacts inside *box* (in local coordinates). The list returned has the same length as *quantiles* */
	static vector<Real> contactCoordQuantiles(const shared_ptr<DemField>& dem, const vector<Real>& quantiles, const shared_ptr<Node>& node, const AlignedBox3r& box);


	static Matrix3i flipCell(const Scene* scene, Matrix3i flip=Matrix3i::Zero());


	#if 0
		static Vector2r radialAxialForce(const Scene* scene, const DemField* dem, int mask, Vector3r axis, bool shear);
	#endif

	# if 0
		// helper zip range adaptor, when psd should iterates over 2 sequences (of diameters and radii)
		// http://stackoverflow.com/a/8513803/761090
		auto zip_end = 
		template <typename... T>
		auto zip(const T&... containers) -> boost::iterator_range<boost::zip_iterator<decltype(boost::make_tuple(std::begin(containers)...))>>{ return boost::make_iterator_range(boost::make_zip_iterator(boost::make_tuple(std::begin(containers)...)),boost::make_zip_iterator(boost::make_tuple(std::end(containers)...))); }
	#endif

	template<class IteratorRange, class DiameterGetter, class WeightGetter> /* iterate over spheres */
	static vector<Vector2r> psd(const IteratorRange& particleRange,
		bool cumulative, bool normalize, int num, Vector2r dRange,
		DiameterGetter diameterGetter,
		WeightGetter weightGetter,
		bool emptyOk=false
	){
		/* determine dRange if not given */
		if(isnan(dRange[0]) || isnan(dRange[1]) || dRange[0]<0 || dRange[1]<=0 || dRange[0]>=dRange[1]){
			dRange=Vector2r(Inf,-Inf); size_t n=0;
			for(const auto& p: particleRange){
				Real d=diameterGetter(p,n);
				if(d<dRange[0]) dRange[0]=d;
				if(d>dRange[1]) dRange[1]=d;
				n+=1;
			}
			if(isinf(dRange[0])){
				if(!emptyOk) throw std::runtime_error("DemFuncs::psd: no spherical particles?");
				else return vector<Vector2r>{Vector2r::Zero(),Vector2r::Zero()};
			}
		}
		/* put particles in bins */
		vector<Vector2r> ret(num,Vector2r::Zero());
		Real weight=0; size_t n=0;
		for(const auto& p: particleRange){
			Real d=diameterGetter(p,n);
			Real w=weightGetter(p,n);
			n+=1;
			// NaN diameter or weight, or zero weight make the particle effectively disappear
			if(isnan(d) || isnan(w) || w==0.) continue;
			// particles beyong upper bound are discarded, though their weight is taken in account
			weight+=w;
			if(d>dRange[1]) continue;
			int bin=max(0,min(num-1,1+(int)((num-1)*((d-dRange[0])/(dRange[1]-dRange[0])))));
			ret[bin][1]+=w;
		}
		// set diameter values
		for(int i=0;i<num;i++) ret[i][0]=dRange[0]+i*(dRange[1]-dRange[0])/(num-1);
		// cummulate and normalize
		if(normalize) for(int i=0;i<num;i++) ret[i][1]=ret[i][1]/weight;
		if(cumulative) for(int i=1;i<num;i++) ret[i][1]+=ret[i-1][1];
		return ret;
	};

	template<class IteratorRange, class ItemGetter>
	static py::object seqVectorToPy(const IteratorRange& range, ItemGetter itemGetter, bool zipped){
		if(!zipped){
			size_t num=decltype(itemGetter(*(range.begin())))::RowsAtCompileTime;
			vector<py::list> ret(num);
			for(const auto& p: range){
				auto item=itemGetter(p);
				for(int i=0; i<item.size(); i++) ret[i].append(item[i]);
			}
			py::list l;
			for(size_t i=0; i<ret.size(); i++) l.append(ret[i]);
			return py::tuple(l);
		} else {
			py::list ret;
			for(const auto& p: range){ ret.append(itemGetter(p)); }
			return ret;
		}
	}

	/* Create facets from STL file; scale, shift and rotation (ori) are applied in this order before
	   creating nodes in global coordinates.

		Both ASCII and binary files are supported; COLOR and MATERIAL values translate to the
		Shape::color scalar; that means that color differences are preserved, but the rendering
		will be different.

		Threshold is distance relative to maximum bounding box size if negative; if positive, it is absolute in the resulting (scaled) space.

		STL color specification (global or per-facet RGB colors) are not yet implemented; a warning is shown unless *readColors* is false.

	*/
	static vector<shared_ptr<Particle>> importSTL(const string& filename, const shared_ptr<Material>& mat, int mask=0, Real color=0., Real scale=1., const Vector3r& shift=Vector3r::Zero(), const Quaternionr& ori=Quaternionr::Identity(), Real threshold=-1e-6, Real maxBox=0, bool readColors=true, bool flex=false, Real thickness=0.);

	#ifdef WOO_VTK
		static bool vtkExportTraces(const shared_ptr<Scene>& scene, const shared_ptr<DemField>& dem, const string& filename, const Vector2i& moduloOffset=Vector2i::Zero());
	#endif

	/* return porosity of particles, compued as void volume fraction after radical Voronoi tesselation */
	static vector<Real> boxPorosity(const shared_ptr<DemField>&, const AlignedBox3r& box);

	static std::map<Particle::id_t,std::vector<Vector3r>> surfParticleIdNormals(const shared_ptr<DemField>& dem, const AlignedBox3r& box, const Real& r);


};


