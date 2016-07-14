#include<woo/pkg/dem/POVRayExport.hpp>

#include<boost/filesystem/operations.hpp>
#include<boost/filesystem/convenience.hpp>
#include<boost/range/join.hpp>

#include<woo/pkg/dem/Sphere.hpp>
#include<woo/pkg/dem/Facet.hpp>
#include<woo/pkg/dem/InfCylinder.hpp>
#include<woo/pkg/dem/Truss.hpp>
#include<woo/pkg/dem/Ellipsoid.hpp>
#include<woo/pkg/dem/Wall.hpp>
#include<woo/pkg/dem/Capsule.hpp>


WOO_PLUGIN(dem,(POVRayExport));
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_POVRayExport__CLASS_BASE_DOC_ATTRS);
WOO_IMPL_LOGGER(POVRayExport);


std::string vec2pov(const Vector3r& v){ return "<"+to_string(v[0])+","+to_string(v[1])+","+to_string(v[2])+">"; }
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
	if(!boost::filesystem::exists(masterPov)) writeMasterPov(masterPov);
	// overwrite static mesh when run the first time always
	if(nDone==1 || !boost::filesystem::exists(staticInc)) writeParticleInc(staticInc,/*doStatic*/true);

	string frameInc=out+(boost::format("_frame_%05d.inc")%frameCounter).str();
	writeParticleInc(frameInc,/*doStatic*/false);
	frameCounter++;
}

void POVRayExport::writeParticleInc(const string& frameInc, bool doStatic){
	DemField* dem=static_cast<DemField*>(field.get());
	std::ofstream os;
	os.open(frameInc);
	if(!os.is_open()) throw std::runtime_error("Unable to open output file '"+frameInc+"'.");
	// write time
	os<<"#declare woo_time="<<std::setprecision(16)<<scene->time<<"; /*current simulation time, in seconds*/\n";
	for(const auto& p: *dem->particles){
		// selection the same as in VtkExport
		if(!p->shape) continue;
		if(mask && !(mask&p->mask)) continue;
		if(doStatic!=!!(staticMask & p->mask)) continue; // skip non-static particles with doStatic and vice versa
		if(!p->shape->getVisible() && skipInvisible) continue;
		if(!clip.isEmpty() && !clip.contains(p->shape->nodes[0]->pos)) continue;
		exportParticle(os,p);
	}
	os.close();
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
			if(aa.angle()>1e-6 && abs(aa.axis()[infCyl->axis])<.999999) LOG_WARN("#"<<p->id<<": InfCylinder rotation not aligned with its axis (cylinder axis "<<infCyl->axis<<"; rotation axis "<<aa.axis()<<", angle "<<aa.angle()<<")");
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
	string staticInc=boost::filesystem::path(out+"_static.inc").filename().string();
	string masterBase=boost::filesystem::path(masterPov).filename().string();
	string texturesInc=boost::filesystem::path(out+"_textures.inc").filename().string();
	string frameIncBase=boost::filesystem::path(out+"_frame_").filename().string();

	if(!os.is_open()) throw std::runtime_error("Unable to open output file '"+masterPov+"'.");
	os<<
		"// Written with Woo ver. "<<BOOST_PP_STRINGIZE(WOO_VERSION)<<" rev. "<<BOOST_PP_STRINGIZE(WOO_REVISION)<<".\n"
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
