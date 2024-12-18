#include"Funcs.hpp"
#include"L6Geom.hpp"
#include"G3Geom.hpp"
#include"FrictMat.hpp"
#include"Clump.hpp"
#include"Ellipsoid.hpp"
#include"Capsule.hpp"
#include"../fem/Membrane.hpp"
#include"DynDt.hpp"
#include"../supp/base/CompUtils.hpp"
#include"../supp/base/CrossPlatform.hpp"

#include<cstdint>
#include<iostream>
#include<fstream>
#include<boost/algorithm/string/predicate.hpp>
#include<boost/range/algorithm/count_if.hpp>
#include<boost/range/algorithm/sort.hpp>
#include<boost/predef/other/endian.h>

#ifdef WOO_VTK
	#include<vtkSmartPointer.h>
	#include<vtkPoints.h>
	#include<vtkPolyData.h>
	#include<vtkIncrementalOctreePointLocator.h>
	#include<vtkPointLocator.h>
#endif

#ifdef WOO_OPENGL
	#include"../gl/Renderer.hpp"
#endif

#include <boost/range/adaptor/filtered.hpp>

WOO_IMPL_LOGGER(DemFuncs);

// to be used in Python wrappers, to get the DEM field
shared_ptr<DemField> DemFuncs::getDemField(const Scene* scene){
	shared_ptr<DemField> ret;
	for(const shared_ptr<Field>& f: scene->fields){
		if(dynamic_pointer_cast<DemField>(f)){
			if(ret) throw std::runtime_error("Ambiguous: more than one DemField in Scene.fields.");
			ret=static_pointer_cast<DemField>(f);
		}
	}
	if(!ret) throw std::runtime_error("No DemField in Scene.fields.");
	return ret;
}

Matrix3i DemFuncs::flipCell(const Scene* scene, Matrix3i flip){
	if(!scene->isPeriodic) throw std::runtime_error("flipCell: scene is not periodic.");
	const auto& hSize(scene->cell->hSize);
	// compute the best flip if unspecified
	if(flip==Matrix3i::Zero()){
		for(int i:{0,1,2}){
			for(int j:{0,1,2}){
				if(i!=j) flip(i,j)=-floor(hSize.col(j).dot(hSize.col(i))/hSize.col(i).squaredNorm());
				else flip(i,j)=0;
			}
		}
		if(flip==Matrix3i::Zero()){ cerr<<"No-op zero flip."<<endl; return flip; }
	}
	if(flip.diagonal()!=Vector3i::Zero()) throw std::runtime_error("flipCell: flip matrix must have zeros on diagonal.");
	cerr<<"Flip matrix (det="<<flip.determinant()<<"):"<<endl<<flip<<endl;
	// compute inverse (applied to cellDist in all contacts
	assert(flip.determinant()==1);
	Matrix3i invFlip=flip.adjoint();
	cerr<<"Inverted flip matrix:"<<endl<<invFlip<<endl;
	// adjust contacts
	for(const auto& C: *(DemFuncs::getDemField(scene)->contacts)) C->cellDist=invFlip*C->cellDist;
	// invalidate collider cache
	bool coll=false;
	for(const auto& e: scene->engines){
		if(e->isA<Collider>()){ coll=true; e->cast<Collider>().invalidatePersistentData(); }
	}
	if(!coll) cerr<<"No collider found after cell flip, garbage ahead?"<<endl;
	// adjust cell parameters
	scene->cell->setHSize(scene->cell->hSize+scene->cell->hSize*flip.cast<Real>());
	// finito
	return flip;
}

/* See https://yade-dem.org/doc/yade.utils.html#yade.utils.avgNumInteractions for docs */
Real DemFuncs::coordNumber(const shared_ptr<DemField>& dem, const shared_ptr<Node>& node, const AlignedBox3r& box, int mask, bool skipFree){
	long C2=0; // twice the number of contacts (counted for each participating particle)
	long N0=0; // number of particles without contact (floaters)
	long N1=0; // number of particles with one contact (rattlers)
	long N=0;  // number of all particles
	for(const auto& p: *dem->particles){
		const Vector3r& pos=p->shape->nodes[0]->pos;
		if(mask && (mask&p->mask)==0) continue;
		if(p->shape->nodes[0]->getData<DemData>().isClumped()) throw std::runtime_error("Not yet implemented for clumps.");
		if(!box.contains(node?node->glob2loc(pos):pos)) continue;
		int n=p->countRealContacts();
		if(n==0) N0++;
		else if(n==1) N1++;
		N++;
		C2+=n;
	}
	if(skipFree) return (C2-N1)*1./(N-N0-N1);
	else return C2*1./N;
}

Real DemFuncs::porosity(const shared_ptr<DemField>& dem, const shared_ptr<Node>& node, const AlignedBox3r& box){
	Real Vs=0; // solid volume
	Real V=box.volume(); // box volume
	for(const auto& p: *dem->particles){
		if(!p->shape->isA<Sphere>()) continue;
		const Vector3r& pos=p->shape->nodes[0]->pos;
		if(!box.contains(node?node->glob2loc(pos):pos)) continue;
		Vs+=(4/3.)*M_PI*pow3(p->shape->cast<Sphere>().radius);
	}
	return Vs/V;
}

vector<Real> DemFuncs::contactCoordQuantiles(const shared_ptr<DemField>& dem, const vector<Real>& quantiles, const shared_ptr<Node>& node, const AlignedBox3r& box){
	vector<Real> ret;
	ret.reserve(quantiles.size());
	if(quantiles.empty()) return ret;
	vector<Real> coords;
	// count contacts first
	// this count will be off when range is given; it will be larger, so copying is avoided at the cost of some spurious allocation
	size_t N;
	#if 0
		N=boost::range::count_if(*dem->contacts,[&](const shared_ptr<Contact>& C)->bool{ return C->isReal(); });
		coords.reserve(N);
	#endif
	for(const shared_ptr<Contact>& C: *dem->contacts){
		if(!C->isReal()) continue;
		Vector3r p=node?node->glob2loc(C->geom->node->pos):C->geom->node->pos;
		if(!box.isEmpty() && !box.contains(p)) continue;
		coords.push_back(p[2]);
	};
	// no contacts yet, return NaNs
	if(coords.empty()){
		for(size_t i=0; i<quantiles.size(); i++) ret.push_back(NaN);
		return ret;
	}
	boost::range::sort(coords);
	N=coords.size(); // should be the same as N before
	for(Real q: quantiles){
		// clamp the value to 0,1
		q=min(max(0.,q),1.);
		if(isnan(q)) q=0.; // just to make sure
		int i=min((int)((N-1)*q),(int)N-1);
		ret.push_back(coords[i]);
	}
	return ret;
}

Real DemFuncs::pWaveDt(const shared_ptr<DemField>& dem, bool noClumps/*=false*/){
	Real dt=Inf;
	for(const shared_ptr<Particle>& p: *dem->particles){
		if(!p || !p->material || !p->shape || p->shape->nodes.size()!=1 || !p->shape->nodes[0]->hasData<DemData>()) continue;
		const auto& n(p->shape->nodes[0]);
		const auto& dyn(n->getData<DemData>());
		const shared_ptr<ElastMat>& elMat=dynamic_pointer_cast<ElastMat>(p->material);
		if(!elMat) continue;
		Real radius;
		if(p->shape->isA<Sphere>()) radius=p->shape->cast<Sphere>().radius;
		else if(p->shape->isA<Ellipsoid>()) radius=p->shape->cast<Ellipsoid>().semiAxes.minCoeff();
		else if(p->shape->isA<Capsule>()) radius=p->shape->cast<Capsule>().radius;
		else continue;
		// for clumps, the velocity is higher: the distance from the sphere center to the clump center
		// is traversed immediately, thus we need to increase the velocity artificially
		Real velMult=1.;
		if(dyn.isClumped() && !noClumps){
			throw std::runtime_error("utils.pWaveDt does not currently work with clumps; pass noClumps=True to ignore clumps (and treat them as spheres) at your own risk.");
			// this is probably bogus:
			// assert(dyn.master); velMult+=(n->pos-dyn.master.lock()->pos).norm()/s->radius;
		}
		dt=min(dt,radius/(velMult*sqrt(elMat->young/elMat->density)));
	}
	return dt;
}

Real DemFuncs::critDt(const shared_ptr<Scene>& scene, const shared_ptr<DemField>& dem, bool noClumps){
	Real dt=min(Inf,DemFuncs::pWaveDt(dem,/*noClumps*/noClumps));
	return dt;
}


std::tuple</*stress*/Matrix3r,/*stiffness*/Matrix6r> DemFuncs::stressStiffness(const Scene* scene, const DemField* dem, bool skipMultinodal, Real volume){
	const int kron[3][3]={{1,0,0},{0,1,0},{0,0,1}}; // Kronecker delta

	Matrix3r stress=Matrix3r::Zero();
	Matrix6r K=Matrix6r::Zero();

	for(const shared_ptr<Contact>& C: *dem->contacts){
		FrictPhys* phys=WOO_CAST<FrictPhys*>(C->phys.get());
		const Particle *pA=C->leakPA(), *pB=C->leakPB();
		Vector3r posB=pB->shape->nodes[0]->pos;
		Vector3r posA=pA->shape->nodes[0]->pos-(scene->isPeriodic?scene->cell->intrShiftPos(C->cellDist):Vector3r::Zero());
		if(pA->shape->nodes.size()!=1 || pB->shape->nodes.size()!=1){
			if(skipMultinodal) continue;
			//else woo::ValueError("Particle "+to_string(pA->shape->nodes.size()!=1? pA->id : pB->id)+" has more than one node; to skip contacts with such particles, say skipMultinodal=True");
			if(pA->shape->nodes.size()!=1) posA=C->geom->node->pos;
			if(pB->shape->nodes.size()!=1) posB=C->geom->node->pos;
		}
		// use current distance here
		const Real d0=(posA-posB).norm();
		Vector3r n=C->geom->node->ori*Vector3r::UnitX(); // normal in global coords
		#if 1
			// g3geom doesn't set local x axis properly
			G3Geom* g3g=dynamic_cast<G3Geom*>(C->geom.get());
			if(g3g) n=g3g->normal;
		#endif
		// contact force, in global coords
		Vector3r F=C->geom->node->ori*C->phys->force;
		Real fN=F.dot(n);
		Vector3r fT=F-n*fN;
		//cerr<<"n="<<n.transpose()<<", fN="<<fN<<", fT="<<fT.transpose()<<endl;
		for(int i:{0,1,2}) for(int j:{0,1,2}) stress(i,j)+=d0*(fN*n[i]*n[j]+.5*(fT[i]*n[j]+fT[j]*n[i]));
		//cerr<<"stress="<<stress<<endl;
		const Real& kN=phys->kn; const Real& kT=phys->kt;
		// only upper triangle used here
		for(int p=0; p<6; p++) for(int q=p;q<6;q++){
			int i=voigtMap[p][q][0], j=voigtMap[p][q][1], k=voigtMap[p][q][2], l=voigtMap[p][q][3];
			K(p,q)+=d0*d0*(kN*n[i]*n[j]*n[k]*n[l]+kT*(.25*(n[j]*n[k]*kron[i][l]+n[j]*n[l]*kron[i][k]+n[i]*n[k]*kron[j][l]+n[i]*n[l]*kron[j][k])-n[i]*n[j]*n[k]*n[l]));
		}
	}
	for(int p=0;p<6;p++)for(int q=p+1;q<6;q++) K(q,p)=K(p,q); // symmetrize
	if(volume<=0){
		if(scene->isPeriodic) volume=scene->cell->getVolume();
		else woo::ValueError("Positive volume value must be given for aperiodic simulations.");
	}
	stress/=volume; K/=volume;
	return std::make_tuple(stress,K);
}

Real DemFuncs::unbalancedForce(const Scene* scene, const DemField* dem, bool useMaxForce){
	// get maximum force on a body and sum of all forces (for averaging)
	Real sumF=0,maxF=0;
	int nb=0;
	for(const shared_ptr<Node>& n: dem->nodes){
		DemData& dyn=n->getData<DemData>();
		if(!dyn.isBlockedNone() || dyn.isClumped()) continue;
		Real currF;
		// we suppose here the clump has not yet received any forces from its members
		// and get additional forces from particles
		if(dyn.isClump()){ 
			Vector3r F(dyn.force), T(Vector3r::Zero()) /*we don't care about torque*/; 
			ClumpData::forceTorqueFromMembers(n,F,T); 
			currF=F.norm();
		} else currF=dyn.force.norm();
		maxF=max(currF,maxF);
		sumF+=currF;
		nb++;
	}
	Real meanF=sumF/nb;
	// get mean force on interactions
	sumF=0; nb=0;
	for(const shared_ptr<Contact>& C: *dem->contacts){
		sumF+=C->phys->force.norm(); nb++;
	}
	sumF/=nb;
	return (useMaxForce?maxF:meanF)/(sumF);
}

bool DemFuncs::particleStress(const shared_ptr<Particle>& p, Vector3r& normal, Vector3r& shear){
	if(!p || !p->shape || p->shape->nodes.size()!=1) return false;
	normal=shear=Vector3r::Zero();
	for(const auto& idC: p->contacts){
		const shared_ptr<Contact>& C(idC.second);
		if(!C->isReal()) continue;
		L6Geom* l6g=dynamic_cast<L6Geom*>(C->geom.get());
		if(!l6g) continue;
		const Vector3r& Fl=C->phys->force;
		normal+=(1/l6g->contA)*l6g->node->ori*Vector3r(Fl[0],0,0); //???
		shear+=(1/l6g->contA)*l6g->node->ori*Vector3r(0,Fl[1],Fl[2]);
	}
	return true;
}

shared_ptr<Particle> DemFuncs::makeSphere(Real radius, const shared_ptr<Material>& m){
	shared_ptr<Shape> sphere=make_shared<Sphere>();
	sphere->cast<Sphere>().radius=radius;
	return Particle::make(sphere,m);
}

vector<Particle::id_t> DemFuncs::SpherePack_toSimulation_fast(const shared_ptr<SpherePack>& sp, const Scene* scene, const DemField* dem, const shared_ptr<Material>& mat, int mask, Real color){
	vector<Particle::id_t> ret; ret.reserve(sp->pack.size());
	if(sp->cellSize!=Vector3r::Zero()) throw std::runtime_error("PBC not supported.");
	for(const auto& s: sp->pack){
		if(s.clumpId>=0) throw std::runtime_error("Clumps not supported.");
		shared_ptr<Particle> p=makeSphere(s.r,mat);
		p->shape->nodes[0]->pos=s.c;
		p->shape->color=(!isnan(color)?color:Mathr::UnitRandom());
		p->mask=mask;
		ret.push_back(dem->particles->insert(p));
	}
	return ret;
}


vector<Vector2r> DemFuncs::boxPsd(const Scene* scene, const DemField* dem, const AlignedBox3r& box, bool mass, int num, int mask, Vector2r rRange){
	bool haveBox=!isnan(box.min()[0]) && !isnan(box.max()[0]);
	return psd(
		*dem->particles|boost::adaptors::filtered([&](const shared_ptr<Particle>&p){ return p && p->shape && p->shape->nodes.size()==1 && (mask?(p->mask&mask):true) && (bool)(dynamic_pointer_cast<woo::Sphere>(p->shape)) && (haveBox?box.contains(p->shape->nodes[0]->pos):true); }),
		/*cumulative*/true,/*normalize*/true,
		num,
		rRange,
		/*diameter getter*/[](const shared_ptr<Particle>&p, const size_t& i) ->Real { return 2.*p->shape->cast<Sphere>().radius; },
		/*weight getter*/[&](const shared_ptr<Particle>&p, const size_t& i) -> Real{ return mass?p->shape->nodes[0]->getData<DemData>().mass:1.; }
	);
}

size_t DemFuncs::reactionInPoint(const Scene* scene, const DemField* dem, int mask, const Vector3r& pt, bool multinodal, Vector3r& force, Vector3r& torque){
	force=torque=Vector3r::Zero();
	size_t ret=0;
	for(const shared_ptr<Particle>& p: *dem->particles){
		if(!(p->mask & mask) || !p->shape) continue;
		ret++;
		const auto& nn=p->shape->nodes;
		for(const auto& n: nn){
			const Vector3r& F(n->getData<DemData>().force), T(n->getData<DemData>().torque);
			force+=F; torque+=(n->pos-pt).cross(F)+T;
		}
		if(multinodal && nn.size()>1){
			// traverse contacts with other particles
			for(const auto& idC: p->contacts){
				const shared_ptr<Contact>& C(idC.second);
				if(!C->isReal()) continue;
				assert(C->geom && C->phys);
				int forceSign=C->forceSign(p); // +1 if we are pA, -1 if pB
				// force and torque at the contact point in global coords
				const auto& n=C->geom->node;
				Vector3r F(n->ori.conjugate()*C->phys->force *forceSign);
				Vector3r T(n->ori.conjugate()*C->phys->torque*forceSign);
				force+=F;
				torque+=(n->pos-pt).cross(F)+T;
			}
		}
	}
	return ret;
}

#if 0
size_t DemFuncs::radialAxialForce(const Scene* scene, const DemField* dem, int mask, Vector3r axis, bool shear, Vector2r& radAxF){
	size_t ret=0;
	radAxF=Vector2r::Zero();
	axis.normalize();
	for(const shared_ptr<Particle>& p: *dem->particles){
		if(!(p->mask & mask) || !p->shape) continue;
		ret++;
		for(const auto& idC: p->contacts){
			const shared_ptr<Contact>& C(idC.second);
			if(!C->isReal()) continue;
			Vector3r F=C->geom->node->ori*((shear?C->phys->force:Vector3r(C->phys->force[0],0,0))*C->forceSign(p));
			Vector3r axF=F.dot(axis);
			radAxF+=Vector2r(axF,(F-axF).norm());
		}
	}
	return ret;
}
#endif



/*
	The following code is based on GPL-licensed K-3d importer module from
	https://github.com/K-3D/k3d/blob/master/modules/stl_io/mesh_reader.cpp

	TODO: read color/material, convert to scalar color in Woo
*/
vector<shared_ptr<Particle>> DemFuncs::importSTL(const string& filename, const shared_ptr<Material>& mat, int mask, Real color, Real scale, const Vector3r& shift, const Quaternionr& ori, Real threshold, Real maxBox, bool readColors, bool flex, Real thickness){
	vector<shared_ptr<Particle>> ret;
	std::ifstream in(filename,std::ios::in|std::ios::binary);
	if(!in) throw std::runtime_error("Error opening "+filename+" for reading (STL import).");

	char buffer[80]; in.read(buffer, 80); in.seekg(0, std::ios::beg);
	bool isAscii=boost::algorithm::starts_with(buffer,"solid");

	// linear array of vertices, each triplet is one face
	// this is filled from ASCII and binary formats as intermediary representation
	
	// coordinate system change using (ori, scale, shift) is done already when reading vertices from the file
	vector<Vector3r> vertices;
	if(isAscii){
		LOG_TRACE("STL: ascii format detected");
		string lineBuf;
		long lineNo=-1;
		int fVertsNum=0;  // number of vertices in this facet (for checking)
		for(woo::safeGetline(in,lineBuf); in; woo::safeGetline(in,lineBuf)){
			lineNo++;
			string tok;
			std::istringstream line(lineBuf);
			line>>tok;
			if(tok=="facet"){
				string tok2; line>>tok2;
				if(tok2!="normal") LOG_WARN("STL: 'normal' expected after 'facet' (line "+to_string(lineNo)+")");
				// we ignore normal values:
				// Vector3r normal; line>>normal.x(); line>>normal.y(); line>>normal.z();
			} else if(tok=="vertex"){
				Vector3r p; line>>p.x(); line>>p.y(); line>>p.z();
				vertices.push_back(ori*(p*scale+shift));
				fVertsNum++;
			} else if(tok=="endfacet"){
				if(fVertsNum!=3){
					LOG_WARN("STL: face has "+to_string(fVertsNum)+" vertices instead of 3, skipping face (line "+to_string(lineNo)+")");
					vertices.resize(vertices.size()-fVertsNum);
				}
				fVertsNum=0;
			}
		}
	} else { // binary format
		/* make sure we're on little-endian machine, because that's what STL uses */
		#ifdef BOOST_ENDIAN_LITTLE_BYTE
			LOG_TRACE("STL: binary format detected");
			char header[80];
			in.read(header,80);
			if(readColors && (boost::algorithm::contains(header,"COLOR=") || boost::algorithm::contains(header,"MATERIAL="))){
				LOG_WARN("STL: global COLOR/MATERIAL not imported (not implemented yet).");
			}
			int32_t numFaces;
			in.read(reinterpret_cast<char*>(&numFaces),sizeof(int32_t));
			LOG_TRACE("binary STL: number of faces {}",numFaces);
			struct bin_face{ // 50 bytes total
				float normal[3], v[9];
				uint16_t color;
			};
			// longer struct is OK (padding?), but not shorter
			static_assert(sizeof(bin_face)>=50,"One face in the STL binary format must be at least 50 bytes long. !?");
			vector<bin_face> faces(numFaces);
			for(int i=0; i<numFaces; i++){
				in.read(reinterpret_cast<char*>(&faces[i]),50);
				const auto& f(faces[i]);
				LOG_TRACE("binary STL: face #{} @ {}: normal ({}, {}, {})",i,(int)in.tellg()-50,f.normal[0],f.normal[1],f.normal[2]);
				LOG_TRACE("  vertex 0 ({}, {}, {})",f.v[0],f.v[1],f.v[2]);
				LOG_TRACE("  vertex 1 ({}, {}, {})",f.v[3],f.v[4],f.v[5]);
				LOG_TRACE("  vertex 2 ({}, {}, {})",f.v[6],f.v[7],f.v[8]);
				LOG_TRACE("  color {}",f.color);
				for(int j:{0,3,6}) vertices.push_back(ori*(Vector3r(f.v[j+0],f.v[j+1],f.v[j+2])*scale+shift));
				if(readColors && f.color!=0) LOG_WARN("STL: face #"+to_string(i)+": color not imported (not implemented yet).");
			}
		#else
			throw std::runtime_error("Binary STL import not supported on big-endian machines.");
		#endif
	}

	assert(vertices.size()%3==0);
	if(vertices.empty()) return ret;

	AlignedBox3r bbox;
	for(const Vector3r& v: vertices) bbox.extend(v);
	if(threshold<0) threshold*=-bbox.sizes().maxCoeff();

	// tesselate faces so that their smalles bbox dimension (in rotated & scaled space) does not exceed maxBox
	// this is useful to avoid faces with voluminous bboxs, creating many spurious potential contacts
	if(maxBox>0){
		for(int i=0; i<(int)vertices.size(); i+=3){
			AlignedBox3r b; // in resulting (simulation) space
			for(int j:{0,1,2}) b.extend(vertices[i+j]);
			if(b.sizes().minCoeff()<=maxBox) continue;
			// too big, tesselate (in STL space)
			vertices.reserve(vertices.size()+9); // 3 new faces will be added
			/*         A
			          +
			         / \
			        /   \
			    CA + --- + AB
			      / \   / \
              /   \ /   \
			  C + --- + --- + B
                   BC

			*/
			Vector3r A(vertices[i]), B(vertices[i+1]), C(vertices[i+2]);  
			Vector3r AB(.5*(A+B)), BC(.5*(B+C)), CA(.5*(C+A));
			vertices[i+1]=AB; vertices[i+2]=CA;
			vertices.push_back(CA); vertices.push_back(BC); vertices.push_back(C );
			vertices.push_back(CA); vertices.push_back(AB); vertices.push_back(BC);
			vertices.push_back(AB); vertices.push_back(B ); vertices.push_back(BC);

			i-=3; // re-run on the same triangle
		}
	}

	// Incremental neighbor search from VTK fails to work, see
	// http://stackoverflow.com/questions/15173310/removing-point-from-cloud-which-are-closer-than-threshold-distance
	#if 0 and defined(WOO_VTK)
		//	http://markmail.org/thread/zrfkbazg3ljed2mj clears up confucion on BuildLocator vs. InitPointInsert
		// in short: call InitPointInsertion with empty point set, and don't call BuildLocator at all

		// point locator for fast point merge lookup
		auto locator=vtkSmartPointer<vtkPointLocator>::New(); // was vtkIncrementalOctreeLocator, but that one crashed
		auto points=vtkSmartPointer<vtkPoints>::New();
		auto polydata=vtkSmartPointer<vtkPolyData>::New();
		// add the first face so that the locator can be built
		// those points are handled specially in the loop below
		//QQ for(int i:{0,1,2}) points->InsertNextPoint(vertices[i].data());
		polydata->SetPoints(points);
		locator->SetDataSet(polydata);
		Real bounds[]={bbox.min()[0],bbox.max()[0],bbox.min()[1],bbox.max()[1],bbox.min()[2],bbox.max()[2]};
		locator->InitPointInsertion(points,bounds);
	#endif

	LOG_TRACE("Vertex merge threshold is {}",threshold);
	vector<shared_ptr<Node>> nodes;
	for(size_t v0=0; v0<vertices.size(); v0+=3){
		size_t vIx[3];
		__attribute__((unused)) bool isNew[3]={false,false,false};
		for(size_t v: {0,1,2}){
			vIx[v]=nodes.size(); // this value means the point was not found
			const Vector3r& pos(vertices[v0+v]);
			#if 0 and defined(WOO_VTK)
				// for the first face, v0==0, pretend the point was not found, so that the node is create below (without inserting additional point)
				//QQ if(v0>0){
					if(points->GetNumberOfPoints()>0){
						double realDist;
						vtkIdType id=locator->FindClosestPointWithinRadius(threshold,pos.data(),realDist);
						if(id>=0) vIx[v]=id; // point was found
					}
					// if not found, keep vIx[v]==nodes.size()
				//QQ };
			#else
				for(size_t i=0; i<nodes.size(); i++){
					if((pos-nodes[i]->pos).squaredNorm()<pow2(threshold)){
						vIx[v]=i;
						break;
					}
				}
			#endif
			// create new node
			if(vIx[v]==nodes.size()){
				auto n=make_shared<Node>();
				n->pos=pos; 
				n->setData<DemData>(make_shared<DemData>());
				// block all DOFs
				n->getData<DemData>().setBlockedAll();
				#ifdef WOO_OPENGL
					// see comment in DemFuncs::makeSphere
					n->setData<GlData>(make_shared<GlData>());
				#endif
				nodes.push_back(n);
				#if 0 and defined(WOO_VTK)
					//QQ if(v0>0){ // don't add points for the first face, which were added to the locator above already
						LOG_TRACE("Face #{}; new vertex {} (number of vertices: {})",v0/3,n->pos,points->GetNumberOfPoints());
						__attribute__((unused)) vtkIdType id=locator->InsertNextPoint(n->pos.data());
						assert(id==nodes.size()-1); // assure same index of node and point
					//QQ }
				#endif
				isNew[v]=true;
			}
		}
		// degenerate facet (due to tolerance), don't add
		if(vIx[0]==vIx[1] || vIx[1]==vIx[2] || vIx[2]==vIx[0]){
			LOG_TRACE("STL: Face#{} is degenerate (vertex indices {},{},{}), skipping.",v0/3,vIx[0],vIx[1],vIx[2]);
			continue;
		}
		LOG_TRACE("STL: Face #{}, node indices {}{}, {}{}, {}{} ({} nodes)",v0/3,vIx[0],(isNew[0]?"*":""),vIx[1],(isNew[1]?"*":""),vIx[2],(isNew[2]?"*":""),nodes.size());
		// create facet
		shared_ptr<Facet> facet;
		if(!flex) facet=make_shared<Facet>();
		else facet=make_shared<Membrane>();
		auto par=make_shared<Particle>();
		par->shape=facet;
		par->material=mat;
		par->mask=mask;
		for(auto ix: vIx){
			facet->nodes.push_back(nodes[ix]);
			nodes[ix]->getData<DemData>().addParRef(par);
		}
		facet->color=color; // set per-face color, once this is imported form the binary STL
		facet->halfThick=thickness/2.;
		ret.push_back(par);
	}
	// compute lumped mass/inertia on nodes
	if(thickness!=0){
		for(const auto& n: nodes) n->getData<DemData>().setOriMassInertia(n);
	}
	return ret;
}


#ifdef WOO_VTK

#include<vtkPolyData.h>
#include<vtkCellArray.h>
#include<vtkPoints.h>
#include<vtkPointData.h>
#include<vtkCellData.h>
#include<vtkPolyLine.h>
#include<vtkDoubleArray.h>
#include<vtkXMLPolyDataWriter.h>
#include<vtkZLibDataCompressor.h>
#include<vtkSmartPointer.h>


#include"Tracer.hpp"


bool DemFuncs::vtkExportTraces(const shared_ptr<Scene>& scene, const shared_ptr<DemField>& dem, const string& filename, const Vector2i& moduloOffset){
	auto polyData=vtkSmartPointer<vtkPolyData>::New();
	auto points=vtkSmartPointer<vtkPoints>::New();
	auto cellArray=vtkSmartPointer<vtkCellArray>::New();
	polyData->SetPoints(points);
	polyData->SetLines(cellArray);

	auto radii=vtkSmartPointer<vtkDoubleArray>::New(); radii->SetNumberOfComponents(1); radii->SetName("radius");
	auto scalars=vtkSmartPointer<vtkDoubleArray>::New(); scalars->SetNumberOfComponents(1);
	string scalarName;
	polyData->GetPointData()->AddArray(scalars);
	polyData->GetCellData()->AddArray(radii); // one radius per entire trace

	auto doOneNode=[&](const shared_ptr<Node>& n){
		const auto& dyn=n->getData<DemData>();
		if(moduloOffset[0]>0 && ((dyn.linIx-moduloOffset[1])%moduloOffset[0])!=0) return;
		if(dyn.parRef.size()>1) return; // skip nodes with more than 1 particle
		if(!n->rep || !n->rep->isA<TraceVisRep>()) return;
		const auto& trace=n->rep->cast<TraceVisRep>();
		// get first usable name for the scalar
		if(scalarName.empty() && trace.tracer && trace.tracer->lineColor) scalarName=trace.tracer->lineColor->label;
		const Particle* p=dyn.parRef.front();
		Real r=0; // deleted particles will have this
		// XXX: only for nodes which have not been deleted yet...
		// XXX: Particle is still stored (DemField::deadParticles),
		// XXX: but the node (DemField::deadNodes) does not have parRef anymore
		if(p && p->shape) r=p->shape->equivRadius();
		if(isnan(r)) return; // skip non-spheroids
		size_t count=trace.countPointData();
		if(count<=1) return;
		// radius
		radii->InsertNextValue(r);
		// trace points
		auto polyLine=vtkSmartPointer<vtkPolyLine>::New();
		polyLine->GetPointIds()->SetNumberOfIds(count);
		for(size_t i=0; i<count; i++){
			polyLine->GetPointIds()->SetId(i,points->GetNumberOfPoints());
			Vector3r pt; Real scalar; Real time;
			trace.getPointData(i,pt,time,scalar);
			points->InsertNextPoint(pt.data());
			scalars->InsertNextValue(scalar);
			// radius->InsertNextValue(r);
		}
		cellArray->InsertNextCell(polyLine);
	};

	for(const auto& n: dem->nodes){ doOneNode(n); }
	for(const auto& n: dem->deadNodes){ doOneNode(n); }

	// don't write anything if there are no traces at all
	if(points->GetNumberOfPoints()==0) return false;

	scalars->SetName(scalarName.c_str()); // hopefully something


	auto writer=vtkSmartPointer<vtkXMLPolyDataWriter>::New();
	bool compress=true; bool ascii=false;
	if(compress) writer->SetCompressor(vtkSmartPointer<vtkZLibDataCompressor>::New());
	if(ascii) writer->SetDataModeToAscii();
	// string fn=out+"con."+to_string(scene->step)+".vtp";
	// string out=filename;
	// if(!boost::algorithm::ends_with(out,".vtp")) out+=".vtp";
	writer->SetFileName(filename.c_str());
	#if VTK_MAJOR_VERSION==5
		writer->SetInput(polyData);
	#else
		writer->SetInputData(polyData);
	#endif
	writer->Write();
	return true;
}

#endif /* WOO_VTK */


#include"../lib/voro++/voro++.hh"


// from voro++ example, http://math.lbl.gov/voro++/examples/irregular/
// Create a wall class that, whenever called, will replace the Voronoi cell
// with a prescribed shape, in this case a dodecahedron
class _wall_initial_shape : public voro::wall {
	public:
		_wall_initial_shape(Real r=1) {
			const double Phi=r*0.5*(1+sqrt(5.0));
			// Create a dodecahedron
			v.init(-2*r,2*r,-2*r,2*r,-2*r,2*r);
			v.plane(0,Phi,r);v.plane(0,-Phi,r);v.plane(0,Phi,-r);
			v.plane(0,-Phi,-r);v.plane(r,0,Phi);v.plane(-r,0,Phi);
			v.plane(r,0,-Phi);v.plane(-r,0,-Phi);v.plane(Phi,r,0);
			v.plane(-Phi,r,0);v.plane(Phi,-r,0);v.plane(-Phi,-r,0);
		};
		bool point_inside(double x,double y,double z) {return true;}
		bool cut_cell(voro::voronoicell &c,double x,double y,double z) {
			// Set the cell to be equal to the dodecahedron
			c=v;
			return true;
		}
		bool cut_cell(voro::voronoicell_neighbor &c,double x,double y,double z) {
			// Set the cell to be equal to the dodecahedron
			c=v;
			return true;
		}
	private:
		voro::voronoicell v;
};


voro::container_poly _makeVoroContainerPolyFromParticlesInBox(const shared_ptr<DemField>& dem, const AlignedBox3r& box, _wall_initial_shape& wis, int idOff=0){
	voro::container_poly con(box.min()[0],box.max()[0],box.min()[1],box.max()[1],box.min()[2],box.max()[2],5,5,5,false,false,false,8);
	// _wall_initial_shape wis(r);
	con.add_wall(wis);
	vector<Particle::id_t> ret;
	for(const auto& p: *dem->particles){
		if(!p->shape) continue;
		const auto& sh(p->shape);
		Real rad=p->shape->equivRadius();
		if(isnan(rad)) continue; // discards also multinodal shapes
		const auto& pos(sh->nodes[0]->pos);
		if(!box.contains(pos)) continue;
		con.put(p->id+idOff,pos[0],pos[1],pos[2],rad);
	}
	return con;
}

vector<Real> DemFuncs::boxPorosity(const shared_ptr<DemField>& dem, const AlignedBox3r& box){
	voro::container_poly con(box.min()[0],box.max()[0],box.min()[1],box.max()[1],box.min()[2],box.max()[2],5,5,5,false,false,false,8);
	_wall_initial_shape wis; // XXX: radius of cell
	con.add_wall(wis);
	vector<Real> ret(dem->particles->size(),NaN); // return array, same ordering as particles; filled with NaN
	for(const auto& p: *dem->particles){
		if(!p->shape) continue;
		const auto& sh(p->shape);
		Real rad=p->shape->equivRadius();
		if(isnan(rad)) continue; // invalid radius (this includes multinodal shapes)
		assert(rad>0); /* tested in Shape::selfTest */
		const auto& pos(sh->nodes[0]->pos);
		if(!box.contains(pos)) continue;
		con.put(p->id,pos[0],pos[1],pos[2],rad);
	}
	voro::c_loop_all cla(con);
	voro::voronoicell c;
	if(cla.start()) do if(con.compute_cell(c,cla)){
		int id; double x,y,z,r;
		cla.pos(id,x,y,z,r);
		const auto& sh=(*dem->particles)[id]->shape;
		ret[id]=1-CompUtils::clamped(sh->volume()/c.volume(),0,1);
	} while(cla.inc());
	return ret;
}

std::map<Particle::id_t,std::vector<Vector3r>> DemFuncs::surfParticleIdNormals(const shared_ptr<DemField>& dem, const AlignedBox3r& box, const Real& r){
	_wall_initial_shape wis(r);
	voro::container_poly con=_makeVoroContainerPolyFromParticlesInBox(dem,box,wis,/*idOff*/1);
	//con.draw_particles_pov("/tmp/surfParticleIds_p.pov");
	//con.draw_cells_pov("/tmp/surfParticleIds_v.pov");
	std::map<Particle::id_t,std::vector<Vector3r>> ret;
	voro::c_loop_all cla(con);
	voro::voronoicell_neighbor c;
	if(cla.start()) do if(con.compute_cell(c,cla)){
		int id; double x,y,z,r;
		cla.pos(id,x,y,z,r);
		std::vector<int> neighbors;
		c.neighbors(neighbors);
		int zeros=0;
		for(const auto& n: neighbors) if(n==0) zeros++;
		if(zeros==0) continue;
		std::vector<double> normals;
		c.normals(normals);
		std::vector<Vector3r> nn;
		nn.reserve(zeros);
		assert(normals.size()/3==neighbors.size());
		for(size_t i=0; i<neighbors.size(); i++){
			if(neighbors[i]==0) nn.push_back(Vector3r(normals[3*i],normals[3*i+1],normals[3*i+2]));
		}
		ret[id-1]=nn;
		#if 0
			cerr<<"#"<<id<<" neighbors ["<<neighbors.size()<<"]: ";
			for(const auto& n: neighbors) cerr<<n<<",";
			cerr<<"   normals["<<normals.size()/3<<"]: ";
			for(size_t i=0; i<normals.size()/3; i+=1) cerr<<normals[3*i]<<","<<normals[3*i+1]<<","<<normals[3*i+2]<<"; ";
			cerr<<"\n";
		#endif
	} while(cla.inc());
	return ret;
}
