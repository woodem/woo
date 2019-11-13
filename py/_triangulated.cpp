#include<woo/core/Master.hpp>

#include<woo/pkg/dem/Particle.hpp>
#include<woo/pkg/dem/Funcs.hpp>
#include<woo/lib/base/CompUtils.hpp>

#include<woo/pkg/dem/Sphere.hpp>
#include<woo/pkg/dem/Capsule.hpp>
#include<woo/pkg/dem/Ellipsoid.hpp>
#include<woo/pkg/dem/VtkExport.hpp>

#include<boost/graph/adjacency_list.hpp>
#include<boost/graph/connected_components.hpp>
#include<boost/graph/subgraph.hpp>
#include<boost/graph/graph_utility.hpp>
#include<boost/property_map/property_map.hpp>

#ifdef WOO_GTS
	#include<gts.h>
#endif

#ifdef WOO_LOG4CXX
	static log4cxx::LoggerPtr logger=log4cxx::Logger::getLogger("woo.triangulated");
#endif

#ifdef WOO_SPDLOG
	static std::shared_ptr<spdlog::logger> logger=spdlog::stdout_color_mt("woo.triangulated");
#endif


int facetsToSTL(const string& out, const shared_ptr<DemField>& dem, const string& solid, int mask, bool append){
	auto particleOk=[&](const shared_ptr<Particle>&p){ return (mask==0 || (p->mask & mask)) && (p->shape->isA<Facet>()); };
	std::ofstream stl(out,append?(std::ofstream::app|std::ofstream::binary):std::ofstream::binary); // binary better, anyway
	if(!stl.good()) throw std::runtime_error("Failed to open output file "+out+" for writing.");
	int num=0;
	stl<<"solid "<<solid<<"\n";
	for(const auto& p: *dem->particles){
		if(!particleOk(p)) continue;
		const auto& f=p->shape->cast<Facet>();
		Vector3r normal=f.getNormal().normalized();
		stl<<"  facet normal "<<normal.x()<<" "<<normal.y()<<" "<<normal.z()<<"\n";
		stl<<"    outer loop\n";
		for(const auto& n: f.nodes) stl<<"      vertex "<<n->pos[0]<<" "<<n->pos[1]<<" "<<n->pos[2]<<"\n";
		stl<<"    endloop\n";
		stl<<"  endfacet\n";
		num+=1;
	}
	stl<<"endsolid "<<solid<<"\n";
	stl.close();
	return num;
}

#ifdef WOO_GTS
	struct _gts_face_to_stl_data{
		_gts_face_to_stl_data(std::ofstream& _stl, Scene* _scene, bool _clipCell, int& _numTri): stl(_stl),scene(_scene),clipCell(_clipCell),numTri(_numTri){};
		std::ofstream& stl;
		const Scene* scene;
		const bool& clipCell;
		int& numTri;
	};
	void _gts_face_to_stl(GtsTriangle* t,_gts_face_to_stl_data* data){
		GtsVertex* v[3];
		Vector3r n;
		gts_triangle_vertices(t,&v[0],&v[1],&v[2]);
		if(data->clipCell && data->scene->isPeriodic){
			for(short ax:{0,1,2}){
				Vector3r p(GTS_POINT(v[ax])->x,GTS_POINT(v[ax])->y,GTS_POINT(v[ax])->z);
				if(!data->scene->cell->isCanonical(p)) return;
			}
		}
		gts_triangle_normal(t,&n[0],&n[1],&n[2]);
		n.normalize();
		std::ofstream& stl(data->stl);
		stl<<"  facet normal "<<n[0]<<" "<<n[1]<<" "<<n[2]<<"\n";
		stl<<"    outer loop\n";
		for(GtsVertex* _v: v){
			stl<<"      vertex "<<GTS_POINT(_v)->x<<" "<<GTS_POINT(_v)->y<<" "<<GTS_POINT(_v)->z<<"\n";
		}
		stl<<"    endloop\n";
		stl<<"  endfacet\n";
		data->numTri+=3;
	}
#endif


int spheroidsToSTL(const string& out, const shared_ptr<DemField>& dem, Real tol, const string& solid, int mask, bool append, bool clipCell, bool merge){
	if(tol==0 || isnan(tol)) throw std::runtime_error("tol must be non-zero.");
	#ifndef WOO_GTS
		if(merge) throw std::runtime_error("woo.triangulated.spheroidsToSTL: merge=True only possible in builds with the 'gts' feature.");
	#endif
	// first traversal to find reference radius
	auto particleOk=[&](const shared_ptr<Particle>&p){ return (mask==0 || (p->mask & mask)) && (p->shape->isA<Sphere>() || p->shape->isA<Ellipsoid>() || p->shape->isA<Capsule>()); };
	int numTri=0;

	if(tol<0){
		LOG_DEBUG("tolerance is negative, taken as relative to minimum radius.");
		Real minRad=Inf;
		for(const auto& p: *dem->particles){
			if(particleOk(p)) minRad=min(minRad,p->shape->equivRadius());
		}
		if(isinf(minRad) || isnan(minRad)) throw std::runtime_error("Minimum radius not found (relative tolerance specified); no matching particles?");
		tol=-minRad*tol;
		LOG_DEBUG("Minimum radius {}.",minRad);
	}
	LOG_DEBUG("Triangulation tolerance is {}",tol);
	
	std::ofstream stl(out,append?(std::ofstream::app|std::ofstream::binary):std::ofstream::binary); // binary better, anyway
	if(!stl.good()) throw std::runtime_error("Failed to open output file "+out+" for writing.");

	Scene* scene=dem->scene;
	if(!scene) throw std::logic_error("DEM field has not associated scene?");

	// periodicity, cache that for later use
	AlignedBox3r cell;

	/*
	wasteful memory-wise, but we need to store the whole triangulation in case *merge* is in effect,
	when it is only an intermediary result and will not be output as-is
	*/
	vector<vector<Vector3r>> ppts;
	vector<vector<Vector3i>> ttri;
	vector<Particle::id_t> iid;

	for(const auto& p: *dem->particles){
		if(!particleOk(p)) continue;
		const auto sphere=dynamic_cast<Sphere*>(p->shape.get());
		const auto ellipsoid=dynamic_cast<Ellipsoid*>(p->shape.get());
		const auto capsule=dynamic_cast<Capsule*>(p->shape.get());
		vector<Vector3r> pts;
		vector<Vector3i> tri;
		if(sphere || ellipsoid){
			Real r=sphere?sphere->radius:ellipsoid->semiAxes.minCoeff();
			// 1 is for icosahedron
			int tess=ceil(M_PI/(5*acos(1-tol/r)));
			LOG_DEBUG("Tesselation level for #{}: {}",p->id,tess);
			tess=max(tess,0);
			auto uSphTri(CompUtils::unitSphereTri20(/*0 for icosahedron*/max(tess-1,0)));
			const auto& uPts=std::get<0>(uSphTri); // unit sphere point coords
			pts.resize(uPts.size());
			const auto& node=(p->shape->nodes[0]);
			Vector3r scale=(sphere?sphere->radius*Vector3r::Ones():ellipsoid->semiAxes);
			for(size_t i=0; i<uPts.size(); i++){
				pts[i]=node->loc2glob(uPts[i].cwiseProduct(scale));
			}
			tri=std::get<1>(uSphTri); // this makes a copy, but we need out own for capsules
		}
		if(capsule){
			#ifdef WOO_VTK
				int subdiv=max(4.,ceil(M_PI/(acos(1-tol/capsule->radius))));
				std::tie(pts,tri)=VtkExport::triangulateCapsule(static_pointer_cast<Capsule>(p->shape),subdiv);
			#else
				throw std::runtime_error("Triangulation of capsules is (for internal and entirely fixable reasons) only available when compiled with the 'vtk' features.");
			#endif
		}
		// do not write out directly, store first for later
		ppts.push_back(pts);
		ttri.push_back(tri);
		LOG_TRACE("#{} triangulated: {},{} faces,vertices.",p->id,tri.size(),pts.size());

		if(scene->isPeriodic){
			// make sure we have aabb, in skewed coords and such
			if(!p->shape->bound){
				// this is a bit ugly, but should do the trick; otherwise we would recompute all that ourselves here
				if(sphere) Bo1_Sphere_Aabb().go(p->shape);
				else if(ellipsoid) Bo1_Ellipsoid_Aabb().go(p->shape);
				else if(capsule) Bo1_Capsule_Aabb().go(p->shape);
			}
			assert(p->shape->bound);
			const AlignedBox3r& box(p->shape->bound->box);
			AlignedBox3r cell(Vector3r::Zero(),scene->cell->getSize()); // possibly in skewed coords
			// central offset
			Vector3i off0;
			scene->cell->canonicalizePt(p->shape->nodes[0]->pos,off0); // computes off0
			Vector3i off; // offset from the original cell
			//cerr<<"#"<<p->id<<" at "<<p->shape->nodes[0]->pos.transpose()<<", off0="<<off0<<endl;
			for(off[0]=off0[0]-1; off[0]<=off0[0]+1; off[0]++) for(off[1]=off0[1]-1; off[1]<=off0[1]+1; off[1]++) for(off[2]=off0[2]-1; off[2]<=off0[2]+1; off[2]++){
				Vector3r dx=scene->cell->intrShiftPos(off);
				//cerr<<"  off="<<off.transpose()<<", dx="<<dx.transpose()<<endl;
				AlignedBox3r boxOff(box); boxOff.translate(dx);
				//cerr<<"  boxOff="<<boxOff.min()<<";"<<boxOff.max()<<" | cell="<<cell.min()<<";"<<cell.max()<<endl;
				if(boxOff.intersection(cell).isEmpty()) continue;
				// copy the entire triangulation, offset by dx
				vector<Vector3r> pts2(pts); for(auto& p: pts2) p+=dx;
				vector<Vector3i> tri2(tri); // same topology
				ppts.push_back(pts2);
				ttri.push_back(tri2);
				LOG_TRACE("  offset {}: #{}: {},{} faces,vertices.",off.transpose(),p->id,tri2.size(),pts2.size());
			}
		}
	}

	if(!merge){
		LOG_DEBUG("Will export (unmerged) {} particles to STL.",ppts.size());
		stl<<"solid "<<solid<<"\n";
		for(size_t i=0; i<ppts.size(); i++){
			const auto& pts(ppts[i]);
			const auto& tri(ttri[i]);
			LOG_TRACE("Exporting {} with {} faces.",i,tri.size());
			for(const Vector3i& t: tri){
				Vector3r pp[]={pts[t[0]],pts[t[1]],pts[t[2]]};
				// skip triangles which are entirely out of the canonical periodic cell
				if(scene->isPeriodic && clipCell && (!scene->cell->isCanonical(pp[0]) && !scene->cell->isCanonical(pp[1]) && !scene->cell->isCanonical(pp[2]))) continue;
				numTri++;
				Vector3r n=(pp[1]-pp[0]).cross(pp[2]-pp[1]).normalized();
				stl<<"  facet normal "<<n.x()<<" "<<n.y()<<" "<<n.z()<<"\n";
				stl<<"    outer loop\n";
				for(auto p: {pp[0],pp[1],pp[2]}){
					stl<<"      vertex "<<p[0]<<" "<<p[1]<<" "<<p[2]<<"\n";
				}
				stl<<"    endloop\n";
				stl<<"  endfacet\n";
			}
		}
		stl<<"endsolid "<<solid<<"\n";
		stl.close();
		return numTri;
	}

#if WOO_GTS
	/*****
	Convert all triangulation to GTS surfaces, find their distances, isolate connected components,
	merge these components incrementally and write to STL
	*****/

	// total number of points
	const size_t N(ppts.size());
	// bounds for collision detection
	struct Bound{
		Bound(Real _coord, int _id, bool _isMin): coord(_coord), id(_id), isMin(_isMin){};
		Bound(): coord(NaN), id(-1), isMin(false){}; // just for allocation
		Real coord;
		int id;
		bool isMin;
		bool operator<(const Bound& b) const { return coord<b.coord; }
	};
	vector<Bound> bounds[3]={vector<Bound>(2*N),vector<Bound>(2*N),vector<Bound>(2*N)};
	/* construct GTS surface objects; all objects must be deleted explicitly! */
	vector<GtsSurface*> ssurf(N);
	vector<vector<GtsVertex*>> vvert(N);
	vector<vector<GtsEdge*>> eedge(N);
	vector<AlignedBox3r> boxes(N);
	for(size_t i=0; i<N; i++){
		LOG_TRACE("** Creating GTS surface for #{}, with {} faces, {} vertices.",i,ttri[i].size(),ppts[i].size());
		AlignedBox3r box;
		// new surface object
		ssurf[i]=gts_surface_new(gts_surface_class(),gts_face_class(),gts_edge_class(),gts_vertex_class());
		// copy over all vertices
		vvert[i].reserve(ppts[i].size());
		eedge[i].reserve(size_t(1.5*ttri[i].size())); // each triangle consumes 1.5 edges, for closed surfs
		for(size_t v=0; v<ppts[i].size(); v++){
			vvert[i].push_back(gts_vertex_new(gts_vertex_class(),ppts[i][v][0],ppts[i][v][1],ppts[i][v][2]));
			box.extend(ppts[i][v]);
		}
		// create faces, and create edges on the fly as needed
		std::map<std::pair<int,int>,int> edgeIndices;
		for(size_t t=0; t<ttri[i].size(); t++){
			//const Vector3i& t(ttri[i][t]);
			//LOG_TRACE("Face with vertices {},{},{}",ttri[i][t][0],ttri[i][t][1],ttri[i][t][2]);
			Vector3i eIxs;
			for(int a:{0,1,2}){
				int A(ttri[i][t][a]), B(ttri[i][t][(a+1)%3]);
				auto AB=std::make_pair(min(A,B),max(A,B));
				auto ABI=edgeIndices.find(AB);
				if(ABI==edgeIndices.end()){ // this edge not created yet
					edgeIndices[AB]=eedge[i].size(); // last index 
					eIxs[a]=eedge[i].size();
					//LOG_TRACE("  New edge #{}: {}--{} (length {})",eIxs[a],A,B,(ppts[i][A]-ppts[i][B]).norm());
					eedge[i].push_back(gts_edge_new(gts_edge_class(),vvert[i][A],vvert[i][B]));
				} else {
					eIxs[a]=ABI->second;
					//LOG_TRACE("  Found edge #{} for {}--{}",ABI->second,A,B);
				}
			}
			//LOG_TRACE("  New face: edges {}--{}--{}",eIxs[0],eIxs[1],eIxs[2]);
			GtsFace* face=gts_face_new(gts_face_class(),eedge[i][eIxs[0]],eedge[i][eIxs[1]],eedge[i][eIxs[2]]);
			gts_surface_add_face(ssurf[i],face);
		}
		// make sure the surface is OK
		if(!gts_surface_is_orientable(ssurf[i])) LOG_ERROR("Surface of #"+to_string(iid[i])+" is not orientable (expect troubles).");
		if(!gts_surface_is_closed(ssurf[i])) LOG_ERROR("Surface of #"+to_string(iid[i])+" is not closed (expect troubles).");
		assert(!gts_surface_is_self_intersecting(ssurf[i]));
		// copy bounds
		LOG_TRACE("Setting bounds of surf #{}",i);
		boxes[i]=box;
		for(int ax:{0,1,2}){
			bounds[ax][2*i+0]=Bound(box.min()[ax],/*id*/i,/*isMin*/true);
			bounds[ax][2*i+1]=Bound(box.max()[ax],/*id*/i,/*isMin*/false);
		}
	}

	/*
	broad-phase collision detection between GTS surfaces
	only those will be probed with exact algorithms below and merged if needed
	*/
	for(int ax:{0,1,2}) std::sort(bounds[ax].begin(),bounds[ax].end());
	vector<Bound>& bb(bounds[0]); // run the search along x-axis, does not matter really
	std::list<std::pair<int,int>> int0; // broad-phase intersections
	for(size_t i=0; i<2*N; i++){
		if(!bb[i].isMin) continue; // only start with lower bound
		// go up to the upper bound, but handle overflow safely (no idea why it would happen here) as well
		for(size_t j=i+1; j<2*N && bb[j].id!=bb[i].id; j++){
			if(bb[j].isMin) continue; // this is handled by symmetry
			#if EIGEN_VERSION_AT_LEAST(3,2,5)
				if(!boxes[bb[i].id].intersects(boxes[bb[j].id])) continue; // no intersection along all axes
			#else
				// old, less elegant
				if(boxes[bb[i].id].intersection(boxes[bb[j].id]).isEmpty()) continue; 
			#endif
			int0.push_back(std::make_pair(min(bb[i].id,bb[j].id),max(bb[i].id,bb[j].id)));
			LOG_TRACE("Broad-phase collision {}+{}",int0.back().first,int0.back().second);
		}
	}

	/*
	narrow-phase collision detection between GTS surface
	this must be done via gts_surface_inter_new, since gts_surface_distance always succeeds
	*/
	std::list<std::pair<int,int>> int1;
	for(const std::pair<int,int> ij: int0){
		LOG_TRACE("Testing narrow-phase collision {}+{}",ij.first,ij.second);
		#if 0
			GtsRange gr1, gr2;
			gts_surface_distance(ssurf[ij.first],ssurf[ij.second],/*delta ??*/(gfloat).2,&gr1,&gr2);
			if(gr1.min>0 && gr2.min>0) continue;
			LOG_TRACE("  GTS reports collision {}+{} (min. distances {}, {}",ij.first,ij.second,gr1.min,gr2.min);
		#else
			GtsSurface *s1(ssurf[ij.first]), *s2(ssurf[ij.second]);
			GNode* t1=gts_bb_tree_surface(s1);
			GNode* t2=gts_bb_tree_surface(s2);
			GtsSurfaceInter* I=gts_surface_inter_new(gts_surface_inter_class(),s1,s2,t1,t2,/*is_open_1*/false,/*is_open_2*/false);
			GSList* l=gts_surface_intersection(s1,s2,t1,t2); // list of edges describing intersection
			int n1=g_slist_length(l);
			// extra check by looking at number of faces of the intersected surface
			#if 1
				GtsSurface* s12=gts_surface_new(gts_surface_class(),gts_face_class(),gts_edge_class(),gts_vertex_class());
				gts_surface_inter_boolean(I,s12,GTS_1_OUT_2);
				gts_surface_inter_boolean(I,s12,GTS_2_OUT_1);
				int n2=gts_surface_face_number(s12);
				gts_object_destroy(GTS_OBJECT(s12));
			#endif
			gts_bb_tree_destroy(t1,TRUE);
			gts_bb_tree_destroy(t2,TRUE);
			gts_object_destroy(GTS_OBJECT(I));
			g_slist_free(l);
			if(n1==0) continue;
			#if 1
				if(n2==0){ LOG_ERROR("n1==0 but n2=={} (no narrow-phase collision)",n2); continue; }
			#endif
			LOG_TRACE("  GTS reports collision {}+{} ({} edges describe the intersection)",ij.first,ij.second,n1);
		#endif
		int1.push_back(ij);
	}
	/*
	connected components on the graph: graph nodes are 0…(N-1), graph edges are in int1
	see http://stackoverflow.com/a/37195784/761090
	*/
	typedef boost::subgraph<boost::adjacency_list<boost::vecS,boost::vecS,boost::undirectedS,boost::property<boost::vertex_index_t,int>,boost::property<boost::edge_index_t,int>>> Graph;
	Graph graph(N);
	for(const auto& ij: int1) boost::add_edge(ij.first,ij.second,graph);
	vector<size_t> clusters(boost::num_vertices(graph));
	size_t numClusters=boost::connected_components(graph,clusters.data());
	for(size_t n=0; n<numClusters; n++){
		// beginning cluster #n
		// first, count how many surfaces are in this cluster; if 1, things are easier
		int numThisCluster=0; int cluster1st=-1;
		for(size_t i=0; i<N; i++){ if(clusters[i]!=n) continue; numThisCluster++; if(cluster1st<0) cluster1st=(int)i; }
		GtsSurface* clusterSurf=NULL;
		LOG_DEBUG("Cluster {} has {} surfaces.",n,numThisCluster);
		if(numThisCluster==1){
			clusterSurf=ssurf[cluster1st]; 
		} else {
			clusterSurf=ssurf[cluster1st]; // surface of the cluster itself
			LOG_TRACE("  Initial cluster surface from {}.",cluster1st);
			/* composed surface */
			for(size_t i=0; i<N; i++){
				if(clusters[i]!=n || ((int)i)==cluster1st) continue;
				LOG_TRACE("   Adding {} to the cluster",i);
				// ssurf[i] now belongs to cluster #n
				// trees need to be rebuild every time anyway, since the merged surface keeps changing in every cycle
				//if(gts_surface_face_number(clusterSurf)==0) LOG_ERROR("clusterSurf has 0 faces.");
				//if(gts_surface_face_number(ssurf[i])==0) LOG_ERROR("Surface #{} has 0 faces.",i);
				GNode* t1=gts_bb_tree_surface(clusterSurf);
				GNode* t2=gts_bb_tree_surface(ssurf[i]);
				GtsSurfaceInter* I=gts_surface_inter_new(gts_surface_inter_class(),clusterSurf,ssurf[i],t1,t2,/*is_open_1*/false,/*is_open_2*/false);
				GtsSurface* merged=gts_surface_new(gts_surface_class(),gts_face_class(),gts_edge_class(),gts_vertex_class());
				gts_surface_inter_boolean(I,merged,GTS_1_OUT_2);
				gts_surface_inter_boolean(I,merged,GTS_2_OUT_1);
				gts_object_destroy(GTS_OBJECT(I));
				gts_bb_tree_destroy(t1,TRUE);
				gts_bb_tree_destroy(t2,TRUE);
				if(gts_surface_face_number(merged)==0){
					LOG_ERROR("Cluster #{}: 0 faces after fusing #{} (why?), adding #{} separately!",n,i,i);
					// this will cause an extra 1-particle cluster to be created
					clusters[i]=numClusters;
					numClusters+=1;
				} else {
					// not from global vectors (cleanup at the end), explicit delete!
					if(clusterSurf!=ssurf[cluster1st]) gts_object_destroy(GTS_OBJECT(clusterSurf));
					clusterSurf=merged;
				}
			}
		}
		#if 0
			LOG_TRACE("  GTS surface cleanups...");
	 		pygts_vertex_cleanup(clusterSurf,.1*tol); // cleanup 10× smaller than tolerance
		   pygts_edge_cleanup(clusterSurf);
	      pygts_face_cleanup(clusterSurf);
		#endif
		LOG_TRACE("  STL: cluster {} output",n);
		stl<<"solid "<<solid<<"_"<<n<<"\n";
		/* output cluster to STL here */
		_gts_face_to_stl_data data(stl,scene,clipCell,numTri);
		gts_surface_foreach_face(clusterSurf,(GtsFunc)_gts_face_to_stl,(gpointer)&data);
		stl<<"endsolid\n";
		if(clusterSurf!=ssurf[cluster1st]) gts_object_destroy(GTS_OBJECT(clusterSurf));
	}
	// this deallocates also edges and vertices
	for(size_t i=0; i<ssurf.size(); i++) gts_object_destroy(GTS_OBJECT(ssurf[i]));
	return numTri;
#endif /* WOO_GTS */
}



py::list porosity(shared_ptr<DemField>& dem, const AlignedBox3r& box){
	vector<Real> poro=DemFuncs::boxPorosity(dem,box);
	py::list ret;
	for(size_t id=0; id<poro.size(); id++){
		if(isnan(poro[id])) continue;
		const auto& sh((*dem->particles)[id]->shape);
		ret.append(py::make_tuple(id,sh->nodes[0]->pos,poro[id]));
	}
	return ret;
}

py::dict surfParticleIdNormals(shared_ptr<DemField>& dem, const AlignedBox3r& box, Real r){
	std::map<Particle::id_t,std::vector<Vector3r>> surf=DemFuncs::surfParticleIdNormals(dem,box,r);
	py::dict ret;
	for(const auto& sn: surf) ret[py::cast(sn.first)]=sn.second;
	return ret;
}

WOO_PYTHON_MODULE(_triangulated);
#ifdef WOO_PYBIND11
PYBIND11_MODULE(_triangulated,mod){
	WOO_SET_DOCSTRING_OPTS;
	mod.attr("__name__")="woo._triangulated";
	#define _PY_MOD mod.
#else
BOOST_PYTHON_MODULE(_triangulated){
	WOO_SET_DOCSTRING_OPTS;
	py::scope().attr("__name__")="woo._triangulated";
	#define _PY_MOD py::
#endif

	_PY_MOD def("spheroidsToSTL",spheroidsToSTL,WOO_PY_ARGS(py::arg("stl"),py::arg("dem"),py::arg("tol"),py::arg("solid")="woo_export",py::arg("mask")=0,py::arg("append")=false,py::arg("cellClip")=false,py::arg("merge")=false),"Export spheroids (:obj:`spheres <woo.dem.Sphere>`, :obj:`capsules <woo.dem.Capsule>`, :obj:`ellipsoids <woo.dem.Ellipsoid>`) to STL file. *tol* is the maximum distance between triangulation and smooth surface; if negative, it is relative to the smallest equivalent radius of particles for export. *mask* (if non-zero) only selects particles with matching :obj:`woo.dem.Particle.mask`. The exported STL ist ASCII.\n\nSpheres and ellipsoids are exported as tesselated icosahedra, with tesselation level determined from *tol*. The maximum error is :math:`e=r\\left(1-\\cos \\frac{2\\pi}{5}\\frac{1}{2}\\frac{1}{n}\\right)` for given tesselation level :math:`n` (1 for icosahedron, each level quadruples the number of triangles), with :math:`r` being the sphere's :obj:`radius <woo.dem.Sphere.radius>` (or ellipsoid's smallest :obj:`semiAxis <woo.dem.Ellipsoid.semiAxes>`); it follows that :math:`n=\\frac{\\pi}{5\\arccos\\left(1-\\frac{e}{r}\\right)}`, where :math:`n` will be rounded up.\n\nCapsules are triangulated in polar coordinates (slices, stacks). The error for regular :math:`n`-gon is :math:`e=r\\left(1-\\cos\\frac{2\\pi}{2n}\\right)` and it follows that :math:`n=\\frac{\\pi}{\\arccos\\left(1-\\frac{e}{r}\\right)}`; the minimum is restricted to be 4, to avoid degenerate shapes.\n\nThe number of facets written to the STL file is returned.\n\nWith periodic boundaries, *clipCell* will cause all triangles entirely outside of the periodic cell to be discarded.\n\n*solid* specified name of ``solid`` inside the STL file; this is useful in conjunction with *append* (which writes at the end of the file) when writing multi-part STL suitable e.g. for `snappyHexMesh <http://www.openfoam.org/docs/user/snappyHexMesh.php>`__.\n\n*merge* will attempt to remove any inner surfaces so that only the external surface is output. Note that this might take considerable time for many particles.");

	_PY_MOD def("facetsToSTL",facetsToSTL,WOO_PY_ARGS(py::arg("stl"),py::arg("dem"),py::arg("solid"),py::arg("mask")=0,py::arg("append")=false),"Export :obj:`facets <woo.dem.Facet>` to STL file. Periodic boundaries are not handled in any special way.");

	_PY_MOD def("porosity",porosity,WOO_PY_ARGS(py::arg("dem"),py::arg("box")),"Return list of `(id,position,porosity)`, where porosity is computed as 1-Vs/Vv, where Vs is particle volume (sphere, capsule, ellipsoid only) and Vv is cell volume using radical Voronoi tesselation around particles. Highly experimental and subject to further changes.");

	_PY_MOD def("surfParticleIdNormals",surfParticleIdNormals,WOO_PY_ARGS(py::arg("dem"),py::arg("box"),py::arg("r")),"Return map of ID->[normal,normal,...] of normals of cell faces belonging to particle #ID which have no neighbors. [EXPERIMENTAL].");

#undef _PY_MOD

};
