#include<woo/pkg/dem/POVRayExport.hpp>

#include<boost/range/join.hpp>
#include<boost/range/algorithm/sort.hpp>
#include<boost/range/algorithm/copy.hpp>
#include<boost/range/algorithm/set_algorithm.hpp>
#include<boost/function_output_iterator.hpp>

#include<boost/graph/adjacency_list.hpp>
#include<boost/graph/connected_components.hpp>
#include<boost/graph/subgraph.hpp>
#include<boost/graph/graph_utility.hpp>
#include<boost/property_map/property_map.hpp>


#include<woo/pkg/dem/Sphere.hpp>
#include<woo/pkg/dem/Facet.hpp>
#include<woo/pkg/dem/InfCylinder.hpp>
#include<woo/pkg/dem/Truss.hpp>
#include<woo/pkg/dem/Ellipsoid.hpp>
#include<woo/pkg/dem/Wall.hpp>
#include<woo/pkg/dem/Capsule.hpp>

#include<woo/pkg/mesh/Mesh.hpp>

#include<iomanip> // std::setprecision


WOO_PLUGIN(dem,(POVRayExport));
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_POVRayExport__CLASS_BASE_DOC_ATTRS);
WOO_IMPL_LOGGER(POVRayExport);


std::string vec2pov(const Vector3r& v){ return "<"+to_string(v[0])+","+to_string(v[1])+","+to_string(v[2])+">"; }
std::string veci2pov(const Vector3i& v){ return "<"+to_string(v[0])+","+to_string(v[1])+","+to_string(v[2])+">"; }
std::string tq2pov(const Vector3r& t, const Quaternionr& q){
	Matrix3r m=q.toRotationMatrix();
	return "matrix <"
		+to_string(m(0,0))+","+to_string(m(0,1))+","+to_string(m(0,2))+", "
		+to_string(m(1,0))+","+to_string(m(1,1))+","+to_string(m(1,2))+", "
		+to_string(m(2,0))+","+to_string(m(2,1))+","+to_string(m(2,2))+", "
		+to_string(t[0])+","+to_string(t[1])+","+to_string(t[2])+">";
}
std::string node2pov(const shared_ptr<Node>& n){ return tq2pov(n->pos,n->ori); }


void POVRayExport::run(){
	// force some color range
	if(!colorRange) colorRange=make_shared<ScalarRange>();
	colorRange->mnmx=Vector2r(0,1); // full range

	out=scene->expandTags(out);
	string staticInc=out+"_static.inc";
	string masterPov=out+"_master.pov";
	if(!filesystem::exists(masterPov)) writeMasterPov(masterPov);
	// overwrite static mesh when run the first time always
	if(nDone==1 || !filesystem::exists(staticInc)) writeParticleInc(staticInc,/*doStatic*/true);

	string frameInc=out+(boost::format("_frame_%05d.inc")%frameCounter).str();
	writeParticleInc(frameInc,/*doStatic*/false);
	frameCounter++;
}

bool POVRayExport::skipParticle(const shared_ptr<Particle>& p, bool doStatic){
	if(!p->shape) return true;
	if(mask && !(mask&p->mask)) return true;
	if(doStatic!=!!(staticMask & p->mask)) return true; // skip non-static particles with doStatic and vice versa
	if(!p->shape->getVisible() && skipInvisible) return true;
	if(!clip.isEmpty() && !clip.contains(p->shape->nodes[0]->pos)) return true;
	return false;
}

bool POVRayExport::writeParticleInc(const string& frameInc, bool doStatic){
	DemField* dem=static_cast<DemField*>(field.get());
	#if 0
		if(doStatic){
			bool todo=false;
			for(const auto& p: *dem->particles){
				if(skipParticle(p,doStatic)) continue;
				if(facetsAsMesh && p->shape->isA<Facet>()) continue;
				todo=true;
				break;
			}
			if(!todo) return false;
		}
	#endif
	std::ofstream os;
	os.open(frameInc);
	if(!os.is_open()) throw std::runtime_error("Unable to open output file '"+frameInc+"'.");
	// write time, non-static only
	if(!doStatic) { os<<"#declare woo_time="<<std::setprecision(16)<<scene->time<<"; /*current simulation time, in seconds*/\n"; }
	bool facetsAsMesh=(doStatic && connMesh>=CONN_MESH_STATIC_ONLY) || (!doStatic && connMesh>=CONN_MESH_ALWAYS);
	if(facetsAsMesh) writeFacetsMeshInc(os,doStatic);
	for(const auto& p: *dem->particles){
		// selection the same as in VtkExport
		if(skipParticle(p,doStatic)) continue;
		if(facetsAsMesh && p->shape->isA<Facet>()) continue; // done in writeFacetsMeshInc
		exportParticle(os,p);
	}
	os.close();
	return true;
}

string POVRayExport::makeTexture(const shared_ptr<Particle>& p, const string& tex){
	// if tex is given, use that one
	string texture=tex.empty()?"default":tex;
	// use "default" or anything matching the particle mask
	if(tex.empty()){
		for(size_t i=0; i<masks.size(); i++){
			if(masks[i] & p->mask){
				if(textures.size()>i) texture=textures[i]; // otherwise leave default
				break;
			}
		}
	}
	// rendering options -- to be enhanced in the future
	string
		rgb0=vec2pov(colorRange->color(max(p->shape->color-colorFuzz,0.))),
		rgb1=vec2pov(colorRange->color(min(p->shape->color+colorFuzz,1.)));
	Real equivDiam=2*p->shape->equivRadius();
	return string(" woo_tex_")+texture+"("+rgb0+","+rgb1+","+to_string(equivDiam)+")";
};



void POVRayExport::exportParticle(std::ofstream& os, const shared_ptr<Particle>& p){
	const auto sphere=dynamic_cast<Sphere*>(p->shape.get());
	const auto capsule=dynamic_cast<Capsule*>(p->shape.get());
	const auto ellipsoid=dynamic_cast<Ellipsoid*>(p->shape.get());
	const auto wall=dynamic_cast<Wall*>(p->shape.get());
	const auto infCyl=dynamic_cast<InfCylinder*>(p->shape.get());
	const auto facet=dynamic_cast<Facet*>(p->shape.get());
	// convenience
	const auto& n0(p->shape->nodes[0]);


	if(sphere) os<<"sphere{ o, "<<sphere->radius<<" "<<makeTexture(p)<<" "<<node2pov(n0)<<" }";
	else if(capsule) os<<"sphere_sweep{ linear_spline, 2, <"<<-.5*capsule->shaft<<",0,0> ,"<<capsule->radius<<", <"<<.5*capsule->shaft<<",0,0> ,"<<capsule->radius<<" "<<makeTexture(p)<<" "<<node2pov(n0)<<" }";
	else if(ellipsoid) os<<"sphere{ o, 1 scale "<<vec2pov(ellipsoid->semiAxes)<<" "<<makeTexture(p)<<" "<<node2pov(n0)<<" }";
	else if(wall){
		if((wall->glAB.isEmpty())){ os<<"plane{ "<<Vector3r::Unit(wall->axis)<<", "<<wall->nodes[0]->pos[wall->axis]; }
		else{ // quad, as mesh2
			Vector3r a; a[wall->axis]=wall->nodes[0]->pos[wall->axis]; Vector3r b(a), c(a), d(a);
			short ax1((wall->axis+1)%3), ax2((wall->axis+2)%3);
			a[ax1]=wall->glAB.min()[0]; a[ax2]=wall->glAB.min()[1];
			b[ax1]=wall->glAB.min()[0]; b[ax2]=wall->glAB.max()[1];
			c[ax1]=wall->glAB.max()[0]; c[ax2]=wall->glAB.min()[1];
			d[ax1]=wall->glAB.max()[0]; d[ax2]=wall->glAB.max()[1];
			os<<"mesh2{ vertex_vectors { 4 "<<vec2pov(a)<<", "<<vec2pov(b)<<", "<<vec2pov(c)<<", "<<vec2pov(d)<<" } face_indices { 2, <0,2,1>, <1,2,3> }";
		}
		os<<makeTexture(p,wallTexture)<<" }"; // will use the default if wallTexture is empty
	}
	else if(infCyl){
		if(isnan(infCyl->glAB.maxCoeff())){ // infinite cylinder, use quadric for this
			Vector3r abc=Vector3r::Ones(); abc[infCyl->axis]=0;
			os<<"quadric{ "<<vec2pov(abc)<<", <0,0,0>, <0,0,0>, -1 "<<makeTexture(p)<<" "<<node2pov(n0)<<" }";
		} else {
			#if 0
			#endif
			// base point (global coords), first center
			Vector3r pBase(n0->pos); pBase[infCyl->axis]+=infCyl->glAB[0];
			// axis vector (global coords), pBase+pAxis is the second center
			Vector3r pAxis(Vector3r::Zero()); pAxis[infCyl->axis]=infCyl->glAB[1]-infCyl->glAB[0];
			// axial rotation; warn if not around the axis
			AngleAxisr aa(n0->ori); Vector3r axRotVecDeg(aa.axis()*aa.angle()*180./M_PI);
			if(aa.angle()>1e-6 && abs(aa.axis()[infCyl->axis])<.999999) LOG_WARN("#{}: InfCylinder rotation not aligned with its axis (cylinder axis {}; rotation axis {}, angle {})",p->id,infCyl->axis,aa.axis(),aa.angle());
			bool open=!cylCapTexture.empty();
			if(open){
				os<<"/*capped cylinder*/";
				Vector3r normal=Vector3r::Unit(infCyl->axis);
				for(int i:{-1,1}){
					os<<"disc{ o, "<<vec2pov(i*normal)<<", "<<infCyl->radius<<" "<<makeTexture(p,cylCapTexture)<<" rotate "<<vec2pov(axRotVecDeg)<<" translate "<<vec2pov(i<0?pBase:(pBase+pAxis).eval());
					os<<" }";
				}
			}
			// length of the cylinder
			Real cylLen=(infCyl->glAB[1]-infCyl->glAB[0]);
			// non-axial rotation, from +x cyl to infCyl->axis (case-by-case)
			Vector3r nonAxRotVec=(infCyl->axis==0?Vector3r::Zero():(infCyl->axis==1?Vector3r(0,0,90):Vector3r(0,-90,0)));
			// cylinder spans +x*cylLen from origin, then rotated, so that the texture is rotated as well
			os<<"cylinder{ o, <"<<cylLen<<",0,0>, "<<infCyl->radius<<(open?" open ":" ")<<" "<<makeTexture(p)<<" rotate "<<vec2pov(nonAxRotVec)<<" rotate "<<vec2pov(axRotVecDeg)<<" translate "<<vec2pov(pBase)<<" }";
		}
	}
	else if(facet){
		// halfThick ignored for now, as well as connectivity
		if(facet->halfThick>0){
			Vector3r dp=facet->getNormal()*facet->halfThick;
			const Vector3r& a(facet->nodes[0]->pos); const Vector3r& b(facet->nodes[1]->pos); const Vector3r& c(facet->nodes[2]->pos);
			os<<"merge{ sphere_sweep{ linear_spline, 4 "<<vec2pov(a)<<", "<<facet->halfThick<<", "<<vec2pov(b)<<", "<<facet->halfThick<<", "<<vec2pov(c)<<", "<<facet->halfThick<<", "<<vec2pov(a)<<", "<<facet->halfThick<<" } triangle{"<<vec2pov(a+dp)<<", "<<vec2pov(b+dp)<<", "<<vec2pov(c+dp)<<" } triangle {"<<vec2pov(a-dp)<<", "<<vec2pov(b-dp)<<", "<<vec2pov(c-dp)<<" } "<<makeTexture(p)<<" }";
		} else {
			os<<"triangle{ "<<vec2pov(facet->nodes[0]->pos)<<", "<<vec2pov(facet->nodes[1]->pos)<<", "<<vec2pov(facet->nodes[2]->pos)<<" "<<makeTexture(p)<<" }";
		}
	}
	os<<" // id="<<p->id<<endl;
}

void POVRayExport::writeMasterPov(const string& masterPov){
	std::ofstream os;
	os.open(masterPov);
	string staticInc=filesystem::path(out+"_static.inc").filename().string();
	string masterBase=filesystem::path(masterPov).filename().string();
	string texturesInc=filesystem::path(out+"_textures.inc").filename().string();
	string frameIncBase=filesystem::path(out+"_frame_").filename().string();

	if(!os.is_open()) throw std::runtime_error("Unable to open output file '"+masterPov+"'.");
	os<<
		"// Written with Woo ver. TODO rev. TODO.\n"  // "<<WOO_VERSION<<" rev. "<<WOO_REVISION<<".\n"
		"// Woo will not overwrite this file as long as it exists.\n\n"
		"// Run something like\n\n"
		"//    povray +H2000 +W1500 -kff300 "<<masterBase<<"\n\n"
		"// to render.\n\n"
		"// Textures below are no-brain defaults, tune to your needs.\n\n"
	;

	os<<
		"#version 3.7;\n"
		"#include \"colors.inc\"\n"
		"#include \"metals.inc\"\n"
		"#include \"textures.inc\"\n"
		"global_settings{ assumed_gamma 2.2 }\n"
		"camera{\n"
		"   location<25,25,15>\n"
		"   sky z\n"
		"   right x*image_width/image_height // right-handed coordinate system\n"
		"   up z\n"
		"   look_at <0,0,0>\n"
		"   angle 45\n"
		"}\n"
		"background{rgb .2}\n"
		"light_source{<-8,-20,30> color rgb .75}\n"
		"light_source{<25,-12,12> color rgb .44}\n"
		"#declare o=<0,0,0>;\n"
		"#declare nan=1e12; /* needed for particle diameter of planes and such... */ \n\n"
		"// texture definitions -- sample only, improve in "<<texturesInc<<" .\n"
	;
	std::set<string> done; // keep track of what was writte, to avoid spurious redefinitions
	// copy (N small)
	vector<string> tt(textures);
	// add all possible textures so that the output always produces at least something
	tt.push_back("default");
	tt.push_back(wallTexture);
	tt.push_back(cylCapTexture);
	for(const string& t: tt){
		if(t.empty() || done.count(t)>0) continue;
		done.insert(t);
		os<<
			"#macro woo_tex_"<<t<<"(rgb0,rgb1,diam)\n"
			"   texture{\n"
			"      pigment {granite scale .2 color_map { [0 color rgb0] [1.0 color rgb rgb1] }}\n"
			"      normal  {marble bump_size .8 scale .2 turbulence .7 }\n"
			"      finish  {reflection .15 specular .5 ambient .5 diffuse .9 phong 1 phong_size 50}\n"
			"   }\n"
			"#end\n\n"
		;
	}
	os<<
		"// custom (better) definitions of textures, loaded if the file exists\n"
		"// macros above are supposed to be redefined in there\n"
		"#if (file_exists(\""<<texturesInc<<"\"))\n"<<
		"   #include \""<<texturesInc<<"\"\n"
		"#end\n\n";
	os<<"#include \""<<staticInc<<"\"\n";
	os<<"#include concat(concat(\""<<frameIncBase<<"\",str(frame_number,-5,0)),\".inc\")\n\n\n";

	os.close();
}


/*
1. traverses all particles and selects only facets
2. their connectivity is determined, the criterion being that 2 nodes are shared
3. connectivity graph is analyzed and connected components are found
4. each connected component is exported as mesh object (component with 1 facet is exported as plain facet)
*/

void POVRayExport::writeFacetsMeshInc(std::ofstream& os, bool doStatic){
	DemField* dem=static_cast<DemField*>(field.get());
	typedef boost::subgraph<boost::adjacency_list<boost::vecS,boost::vecS,boost::undirectedS,boost::property<boost::vertex_index_t,Particle::id_t>,boost::property<boost::edge_index_t,int>>> Graph;
	// use Particle::id as identifier for graph nodes
	// however, facets won't have contiguous numbers and ids will have to be filtered again
	// as non-facets will be reported the same as single (unconnected) facets
	size_t N=dem->particles->size();
	Graph graph(N);
	for(const auto& p: *dem->particles){
		if(skipParticle(p,doStatic)) continue;
		if(!p->shape->isA<Facet>()) continue;
		// this particle is ready for export
		const auto& f(p->shape->cast<Facet>());
		std::vector<Node*> fnnn={f.nodes[0].get(),f.nodes[1].get(),f.nodes[2].get()};
		boost::range::sort(fnnn);
		// find neighbors sharing 2 nodes
		for(int vert:{0,1,2}){
			for(const Particle* p2: f.nodes[vert]->getData<DemData>().parRef){
				if(!p2->shape || !p2->shape->isA<Facet>() || p2==p.get()) continue;
				const auto& f2(p2->shape->cast<Facet>());
				std::vector<Node*> f2nnn={f2.nodes[0].get(),f2.nodes[1].get(),f2.nodes[2].get()};
				boost::range::sort(f2nnn);
				short numShared=0;
				// http://stackoverflow.com/a/38720126/761090
				boost::range::set_intersection(fnnn,f2nnn,boost::make_function_output_iterator([&](Node*){numShared++;}));
				// not an edge neighbor
				if(numShared!=2) continue;
				boost::add_edge(p->id,p2->id,graph);
			}
		}
	}
	vector<size_t> clusters(boost::num_vertices(graph));
	size_t numClusters=boost::connected_components(graph,clusters.data());
	for(size_t n=0; n<numClusters; n++){
		std::list<Particle::id_t> pp;
		for(size_t i=0; i<N; i++){
			if(clusters[i]!=n) continue;
			const auto& p((*dem->particles)[n]);
			if(!p || skipParticle(p,doStatic) || !p->shape->isA<Facet>()) continue;
			pp.push_back(i);
		}
		if(pp.size()==0) continue;
		// single particle, no mesh; use the normal routine
		if(pp.size()==1) exportParticle(os,(*dem->particles)[pp.front()]);
		else{
			// gather all nodes we need
			std::set<Node*> nodeset;
			for(const auto& id: pp){
				for(int ni:{0,1,2}){
					const auto& n((*dem->particles)[id]->shape->nodes[ni]);
					nodeset.insert(n.get());
				}
			}
			// put them into flat array, and define mapping form Node* to index in that array
			vector<Node*> nodevec; nodevec.reserve(nodeset.size());
			boost::range::copy(nodeset,std::back_inserter(nodevec));
			std::map<Node*,int> node2vertMap;
			for(size_t i=0; i<nodevec.size(); i++) node2vertMap[nodevec[i]]=i;
			vector<list<int>> nodeIxFaces(nodevec.size());

			vector<Vector2r> vert_uv; vert_uv.reserve(nodevec.size()); // one per vertex (if less, will not be used)
			for(const auto& n: nodevec){
				if(n->hasData<MeshData>() && !isnan(n->getData<MeshData>().uvCoord.maxCoeff())) vert_uv.push_back(n->getData<MeshData>().uvCoord);
			}

			vector<Vector3i> faces; // indices in nodevec, 3 per face

			for(const auto& id: pp){
				Vector3i face;
				const auto& f=(*dem->particles)[id]->shape->cast<Facet>();
				for(int ni:{0,1,2}){
					face[ni]=node2vertMap.at(f.nodes[ni].get());
					nodeIxFaces[face[ni]].push_back(faces.size());
				}
				faces.push_back(face);
			}

			// compute vertex normals for all vertices (same ordering)
			// where vertex normal is not used, extra items will be added on-demand
			auto faceNormal=[&nodevec](const Vector3i& f)->Vector3r{ Vector3r v[3]={nodevec[f[0]]->pos,nodevec[f[1]]->pos,nodevec[f[2]]->pos}; return (v[1]-v[0]).cross(v[2]-v[1]).normalized(); };
			auto avgNormal=[&](int vert)->Vector3r{
				Vector3r ret(Vector3r::Zero());
				for(const int fIx: nodeIxFaces[vert]){
					ret+=faceNormal(faces[fIx]);
				}
				return ret/nodeIxFaces[vert].size();
			};
			vector<Vector3r> normals; normals.reserve(nodevec.size());
			// mapping from vertex index to normal index; maps to -1 for vertices without normals
			vector<int> vertexIx2normalIx(nodevec.size(),-1);
			for(size_t ni=0; ni<nodevec.size(); ni++){
				const auto& n(nodevec[ni]);
				if(!n->hasData<MeshData>() || n->getData<MeshData>().vNorm!=MeshData::VNORM_ALL) continue;
				vertexIx2normalIx[ni]=normals.size();
				normals.push_back(avgNormal(ni));
			}
			// if normals.empty(), then there are no vertex normals defined at all, and the whole vertex normal machinery will be skipped and not exported

			vector<Vector3i> normalIx; 
			if(normals.size()>0){
				normalIx.reserve(faces.size());
				for(const auto& f: faces){
					bool fnUsed=false;
					Vector3i nix;
					for(int vi:{0,1,2}){
						// if the vertex has normal defined, use it; if not, add face normal to normals and use it
						if(vertexIx2normalIx[f[vi]]>0) nix[vi]=vertexIx2normalIx[f[vi]];
						else { nix[vi]=normals.size(); fnUsed=true; }
					}
					if(fnUsed) normals.push_back(faceNormal(f));
					normalIx.push_back(nix);
				}
			}

			os<<"mesh2{\n   vertex_vectors { "<<nodevec.size();
			for(const Node* n: nodevec){ os<<", "<<vec2pov(n->pos); }
			os<<" }\n";

			if(nodevec.size()==vert_uv.size()){
				os<<"   uv_vectors { "<<vert_uv.size();
				for(const auto& uv: vert_uv){ os<<", "<<"<"<<to_string(uv[0])<<","<<to_string(uv[1])<<">";}
				os<<" }\n";
			} else {
				if(!vert_uv.empty()) LOG_WARN("Not exporting uv for mesh with {} vertices but only {} with uvCoord defined.",nodevec.size(),vert_uv.size());
			}

			if(!normals.empty()){
				os<<"   normal_vectors { "<<normals.size();
				for(const auto& n: normals) os<<", "<<vec2pov(n);
				os<<" }\n";
			}

			os<<"   face_indices { "<<faces.size();
			for(const auto& f: faces){ os<<", "<<veci2pov(f); }
			os<<" }\n";

			if(!normals.empty()){
				os<<"   normal_indices { "<<faces.size();
				for(const auto& nix: normalIx) os<<", "<<veci2pov(nix);
				os<<" }\n";
			}

			const auto& pp0((*dem->particles)[pp.front()]);
			if(nodevec.size()==vert_uv.size()){
				//os<<"   uv_mapping\n";
				os<<"   /* uv_mapping can be used in the texture function */\n";
			}
			os<<"   "<<makeTexture(pp0,"mesh")<<"\n";
			os<<"}\n";
		}
	}
}


