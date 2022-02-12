// 2009 © Václav Šmilauer <eudoxos@arcig.cz>
#include"../src/supp/base/Logging.hpp"
#include"../src/supp/base/Math.hpp"
#include"../src/supp/base/Types.hpp"
#include"../src/supp/pyutil/doc_opts.hpp"
#include"../src/supp/sphere-pack/SpherePack.hpp"
#include"../src/core/Master.hpp"

/*
This file contains various predicates that say whether a given point is within the solid,
or, not closer than "pad" to its boundary, if pad is nonzero
Besides the (point,pad) operator, each predicate defines aabb() method that returns
(min,max) tuple defining minimum and maximum point of axis-aligned bounding box 
for the predicate.

These classes are primarily used for woo.pack.* functions creating packings.
See examples/regular-sphere-pack/regular-sphere-pack.py for an example.

*/

/* Since we want to make Predicate::operator() and Predicate::aabb() callable from c++ on py::object
with the right virtual method resolution, we have to wrap the class in the following way. See 
http://www.boost.org/doc/libs/1_38_0/libs/python/doc/tutorial/doc/html/python/exposing.html for documentation
on exposing virtual methods.

This makes it possible to derive a python class from Predicate, override its aabb() method, for instance,
and use it in PredicateUnion, which will call the python implementation of aabb() as it should. This
approach is used in the inGtsSurface class defined in pack.py.

See scripts/test/gts-operators.py for an example.

NOTE: you still have to call base class ctor in your class' ctor derived in python, e.g.
super(inGtsSurface,self).__init__() so that virtual methods work as expected.
*/

struct PredicateWrap: public Predicate{
	using Predicate::Predicate;
	bool operator()(const Vector3r& pt, Real pad=0.) const override {
		PYBIND11_OVERLOAD_PURE_NAME(/*return type*/bool,/*parent class*/Predicate,/*python name*/"__call__",/*c++ function*/operator(),/*args*/pt,pad);
	}
	AlignedBox3r aabb() const override {
		PYBIND11_OVERLOAD_PURE(/*return type*/AlignedBox3r,/*parent class*/Predicate,/*function*/aabb);
	}
};

/*********************************************************************************
****************** Boolean operations on predicates ******************************
*********************************************************************************/

//const Predicate& obj2pred(py::object obj){ return py::extract<const Predicate&>(obj)();}

class PredicateBoolean: public Predicate{
	protected:
		const shared_ptr<Predicate> A,B;
	public:
		PredicateBoolean(const shared_ptr<Predicate>& _A, const shared_ptr<Predicate>& _B): A(_A), B(_B){}
		virtual ~PredicateBoolean()=default;
		const shared_ptr<Predicate> getA(){ return A;}
		const shared_ptr<Predicate> getB(){ return B;}
};

// http://www.linuxtopia.org/online_books/programming_books/python_programming/python_ch16s03.html
class PredicateUnion: public PredicateBoolean{
	public:
		PredicateUnion(const shared_ptr<Predicate>& _A, const shared_ptr<Predicate>& _B): PredicateBoolean(_A,_B){}
		bool operator()(const Vector3r& pt,Real pad) const override {return (*A)(pt,pad)||(*B)(pt,pad);}
		AlignedBox3r aabb() const override { return A->aabb().merged(B->aabb()); }
};
PredicateUnion makeUnion(const shared_ptr<Predicate>& A, const shared_ptr<Predicate>& B){ return PredicateUnion(A,B);}

class PredicateIntersection: public PredicateBoolean{
	public:
		PredicateIntersection(const shared_ptr<Predicate>& _A, const shared_ptr<Predicate>& _B): PredicateBoolean(_A,_B){}
		bool operator()(const Vector3r& pt,Real pad) const override {return (*A)(pt,pad) && (*B)(pt,pad);}
		AlignedBox3r aabb() const override { return AlignedBox3r(A->aabb()).intersection(B->aabb()); }
};
PredicateIntersection makeIntersection(const shared_ptr<Predicate>& A, const shared_ptr<Predicate>& B){ return PredicateIntersection(A,B);}

class PredicateDifference: public PredicateBoolean{
	public:
		PredicateDifference(const shared_ptr<Predicate>& _A, const shared_ptr<Predicate>& _B): PredicateBoolean(_A,_B){}
		bool operator()(const Vector3r& pt,Real pad) const override {return (*A)(pt,pad) && !(*B)(pt,-pad);}
		AlignedBox3r aabb() const override { return A->aabb(); }
};
PredicateDifference makeDifference(const shared_ptr<Predicate>& A, const shared_ptr<Predicate>& B){ return PredicateDifference(A,B);}

class PredicateSymmetricDifference: public PredicateBoolean{
	public:
		PredicateSymmetricDifference(const shared_ptr<Predicate>& _A, const shared_ptr<Predicate>& _B): PredicateBoolean(_A,_B){}
		bool operator()(const Vector3r& pt,Real pad) const override {bool inA=(*A)(pt,pad), inB=(*B)(pt,pad); return (inA && !inB) || (!inA && inB);}
		AlignedBox3r aabb() const override { return AlignedBox3r(A->aabb()).extend(B->aabb()); }
};
PredicateSymmetricDifference makeSymmetricDifference(const shared_ptr<Predicate>& A, const shared_ptr<Predicate>& B){ return PredicateSymmetricDifference(A,B);}

/*********************************************************************************
****************************** Primitive predicates ******************************
*********************************************************************************/


/*! Sphere predicate */
class inSphere: public Predicate {
	Vector3r center; Real radius;
public:
	inSphere(const Vector3r& _center, Real _radius){center=_center; radius=_radius;}
	bool operator()(const Vector3r& pt, Real pad=0.) const override { return ((pt-center).norm()-pad<=radius-pad); }
	AlignedBox3r aabb() const override {return AlignedBox3r(Vector3r(center[0]-radius,center[1]-radius,center[2]-radius),Vector3r(center[0]+radius,center[1]+radius,center[2]+radius));}
};

class inAlignedHalfspace: public Predicate{
	short axis; Real coord; bool lower;
public:
	inAlignedHalfspace(int _axis, const Real& _coord, bool _lower=true): axis(_axis), coord(_coord), lower(_lower){
		if(!(axis==0 || axis==1 || axis==2)) throw std::runtime_error("inAlignedHalfSpace.axis: must be in {0,1,2} (not "+to_string(axis)+")");
	}
	bool operator()(const Vector3r& pt, Real pad=0.) const override {
		if(lower){
			return pt[axis]+pad<coord;
		} else {
			return pt[axis]-pad>coord;
		}
	}
	AlignedBox3r aabb() const override { AlignedBox3r ret(Vector3r(-Inf,-Inf,-Inf),Vector3r(Inf,Inf,Inf)); if(lower) ret.max()[axis]=coord; else ret.min()[axis]=coord; return ret; }
};

class inAxisRange: public Predicate{
	short axis; Vector2r range;
	public:
		inAxisRange(int _axis, const Vector2r& _range): axis(_axis), range(_range ){
			if(!(axis==0 || axis==1 || axis==2)) throw std::runtime_error("inAxisRange.axis: must be in {0,1,2} (not "+to_string(axis)+")");
		}
		bool operator()(const Vector3r& pt, Real pad=0.) const override {
			return (pt[axis]-pad>=range[0] && pt[axis]+pad<=range[1]);
		}
		AlignedBox3r aabb() const override { AlignedBox3r ret(Vector3r(-Inf,-Inf,-Inf),Vector3r(Inf,Inf,Inf)); ret.min()[axis]=range[0]; ret.max()[axis]=range[1]; return ret; }
};

/*! Axis-aligned box predicate */
class inAlignedBox: public Predicate{
	Vector3r mn, mx;
public:
	inAlignedBox(const Vector3r& _mn, const Vector3r& _mx): mn(_mn), mx(_mx) {}
	inAlignedBox(const AlignedBox3r& box): mn(box.min()), mx(box.max()) {}
	bool operator()(const Vector3r& pt, Real pad=0.) const override {
		return
			mn[0]+pad<=pt[0] && mx[0]-pad>=pt[0] &&
			mn[1]+pad<=pt[1] && mx[1]-pad>=pt[1] &&
			mn[2]+pad<=pt[2] && mx[2]-pad>=pt[2];
	}
	AlignedBox3r aabb() const override { return AlignedBox3r(mn,mx); }
};

class inOrientedBox: public Predicate{
	Vector3r pos;
	Quaternionr ori;
	AlignedBox3r box;
public:
	inOrientedBox(const Vector3r& _pos, const Quaternionr& _ori, const AlignedBox3r& _box): pos(_pos),ori(_ori),box(_box){};
	bool operator()(const Vector3r& pt, Real pad=0.) const override{
		// shrunk box
		AlignedBox3r box2; box2.min()=box.min()+Vector3r::Ones()*pad; box2.max()=box.max()-Vector3r::Ones()*pad;
		return box2.contains(ori.conjugate()*(pt-pos));
	}
	AlignedBox3r aabb() const override {
		AlignedBox3r ret;
		// box containing all corners after transformation
		for(const auto& corner: {AlignedBox3r::BottomLeftFloor, AlignedBox3r::BottomRightFloor, AlignedBox3r::TopLeftFloor, AlignedBox3r::TopRightFloor, AlignedBox3r::BottomLeftCeil, AlignedBox3r::BottomRightCeil, AlignedBox3r::TopLeftCeil, AlignedBox3r::TopRightCeil}) ret.extend(pos+ori*box.corner(corner));
		return ret;
	}
};

class inParallelepiped: public Predicate{
	Vector3r n[6]; // outer normals, for -x, +x, -y, +y, -z, +z
	Vector3r pts[6]; // points on planes
	Vector3r mn,mx;
public:
	inParallelepiped(const Vector3r& o, const Vector3r& a, const Vector3r& b, const Vector3r& c){
		Vector3r A(o), B(a), C(a+(b-o)), D(b), E(c), F(c+(a-o)), G(c+(a-o)+(b-o)), H(c+(b-o));
		Vector3r x(B-A), y(D-A), z(E-A);
		n[0]=-y.cross(z).normalized(); n[1]=-n[0]; pts[0]=A; pts[1]=B;
		n[2]=-z.cross(x).normalized(); n[3]=-n[2]; pts[2]=A; pts[3]=D;
		n[4]=-x.cross(y).normalized(); n[5]=-n[4]; pts[4]=A; pts[5]=E;
		// bounding box
		Vector3r vertices[8]={A,B,C,D,E,F,G,H};
		mn=mx=vertices[0];
		for(int i=1; i<8; i++){ mn=mn.array().min(vertices[i].array()).matrix(); mx=mx.array().max(vertices[i].array()).matrix(); }
	}
	bool operator()(const Vector3r& pt, Real pad=0.) const override {
		for(int i=0; i<6; i++) if((pt-pts[i]).dot(n[i])>-pad) return false;
		return true;
	}
	AlignedBox3r aabb() const override { return AlignedBox3r(mn,mx); }
};

/*! Arbitrarily oriented cylinder predicate */
class inCylinder: public Predicate{
	Vector3r c1,c2,c12; Real radius,ht;
public:
	inCylinder(const Vector3r& _c1, const Vector3r& _c2, Real _radius){c1=_c1; c2=_c2; c12=c2-c1; radius=_radius; ht=c12.norm(); }
	bool operator()(const Vector3r& pt, Real pad=0.) const {
		Real u=(pt.dot(c12)-c1.dot(c12))/(ht*ht); // normalized coordinate along the c1--c2 axis
		if((u*ht<0+pad) || (u*ht>ht-pad)) return false; // out of cylinder along the axis
		Real axisDist=((pt-c1).cross(pt-c2)).norm()/ht;
		if(axisDist>radius-pad) return false;
		return true;
	}
	AlignedBox3r aabb() const {
		// see http://www.gamedev.net/community/forums/topic.asp?topic_id=338522&forum_id=20&gforum_id=0 for the algorithm
		const Vector3r& A(c1); const Vector3r& B(c2); 
		Vector3r k(
			sqrt((pow2(A[1]-B[1])+pow2(A[2]-B[2])))/ht,
			sqrt((pow2(A[0]-B[0])+pow2(A[2]-B[2])))/ht,
			sqrt((pow2(A[0]-B[0])+pow2(A[1]-B[1])))/ht);
		Vector3r mn=A.array().min(B.array()).matrix(), mx=A.array().max(B.array()).matrix();
		return AlignedBox3r((mn-radius*k).eval(),(mx+radius*k).eval());
	}
	string __str__() const {
		std::ostringstream oss;
		oss<<"<woo.pack.inCylinder @ "<<this<<", A="<<c1.transpose()<<", B="<<c2.transpose()<<", radius="<<radius<<">";
		return oss.str();
	}
};

class inCylSector: public Predicate{
	Vector3r pos; Quaternionr ori; Vector2r rrho, ttheta, zz; Quaternionr oriConj;
	bool limZ, limTh, limRho;
public:
	inCylSector(const Vector3r& _pos, const Quaternionr& _ori, const Vector2r& _rrho=Vector2r::Zero(), const Vector2r& _ttheta=Vector2r::Zero(), const Vector2r& _zz=Vector2r::Zero()): pos(_pos), ori(_ori), rrho(_rrho), ttheta(_ttheta), zz(_zz) {
		oriConj=ori.conjugate();
		for(int i:{0,1}) ttheta[i]=CompUtils::wrapNum(ttheta[i],2*M_PI);
		limRho=(rrho!=Vector2r::Zero());
		limTh=(ttheta!=Vector2r::Zero());
		limZ=(zz!=Vector2r::Zero());
	};
	bool operator()(const Vector3r& pt, Real pad=0.) const {
		Vector3r l3=oriConj*(pt-pos);
		Real z=l3[2];
		if(limZ && (z-pad<zz[0] || z+pad>zz[1])) return false;
		Real th=CompUtils::wrapNum(atan2(l3[1],l3[0]),2*M_PI);
		Real rho=l3.head<2>().norm();
		if(limRho && (rho-pad<rrho[0] || rho+pad>rrho[1])) return false;
		if(limTh){
			Real t0=CompUtils::wrapNum(ttheta[0]+(rho>0?pad/rho:0.),2*M_PI);
			Real t1=CompUtils::wrapNum(ttheta[1]-(rho>0?pad/rho:0.),2*M_PI);
			// we are wrapping around 2π
			// |===t1........t0===|
			if(t1<t0) return (th>=t1 || th<=t0);
			// |...t0========t1...|
			return (th>=t0 && th<=t1);
		};
		return true;
	}
	AlignedBox3r aabb() const {
		// this aabb is very pessimistic, but probably not really needed
		// if(!limRho || !limTh || !limZ) 
		return AlignedBox3r(Vector3r(-Inf,-Inf,-Inf),Vector3r(Inf,Inf,Inf));
		#if 0
			AlignedBox3r bb;
			auto cyl2glob=[&](const Real& rho, const Real& theta, const Real& z)->Vector3r{ return ori*Vector3r(rho*cos(theta),rho*sin(theta),z)+pos; }
			for(Real z:{zz[0],zz[1]){
				bb.extend(cyl2glob(0,0,z)); // center
				bb.extend(cyl2glob(
			}
		#endif
	};
};

/*! Oriented hyperboloid predicate (cylinder as special case).

See http://mathworld.wolfram.com/Hyperboloid.html for the parametrization and meaning of symbols
*/
class inHyperboloid: public Predicate{
	Vector3r c1,c2,c12; Real R,a,ht,c;
public:
	inHyperboloid(const Vector3r& _c1, const Vector3r& _c2, Real _R, Real _r){
		c1=_c1; c2=_c2; R=_R; a=_r;
		c12=c2-c1; ht=c12.norm();
		Real uMax=sqrt(pow2(R/a)-1); c=ht/(2*uMax);
	}
	// WARN: this is not accurate, since padding is taken as perpendicular to the axis, not the the surface
	bool operator()(const Vector3r& pt, Real pad=0.) const override {
		Real v=(pt.dot(c12)-c1.dot(c12))/(ht*ht); // normalized coordinate along the c1--c2 axis
		if((v*ht<0+pad) || (v*ht>ht-pad)) return false; // out of cylinder along the axis
		Real u=(v-.5)*ht/c; // u from the wolfram parametrization; u is 0 in the center
		Real rHere=a*sqrt(1+u*u); // pad is taken perpendicular to the axis, not to the surface (inaccurate)
		Real axisDist=((pt-c1).cross(pt-c2)).norm()/ht;
		if(axisDist>rHere-pad) return false;
		return true;
	}
	AlignedBox3r aabb() const override {
		// the lazy way
		return inCylinder(c1,c2,R).aabb();
	}
};

/*! Axis-aligned ellipsoid predicate */
class inEllipsoid: public Predicate{
	Vector3r c, abc;
public:
	inEllipsoid(const Vector3r& _c, const Vector3r& _abc) {c=_c; abc=_abc;}
	bool operator()(const Vector3r& pt, Real pad=0.) const override {
		//Define the ellipsoid X-coordinate of given Y and Z
		Real x = sqrt((1-pow2((pt[1]-c[1]))/((abc[1]-pad)*(abc[1]-pad))-pow2((pt[2]-c[2]))/((abc[2]-pad)*(abc[2]-pad)))*((abc[0]-pad)*(abc[0]-pad)))+c[0]; 
		Vector3r edgeEllipsoid(x,pt[1],pt[2]); // create a vector of these 3 coordinates
		//check whether given coordinates lie inside ellipsoid or not
		if ((pt-c).norm()<=(edgeEllipsoid-c).norm()) return true;
		else return false;
	}
	AlignedBox3r aabb() const override {
		const Vector3r& center(c); const Vector3r& ABC(abc);
		return AlignedBox3r(Vector3r(center[0]-ABC[0],center[1]-ABC[1],center[2]-ABC[2]),Vector3r(center[0]+ABC[0],center[1]+ABC[1],center[2]+ABC[2]));
	}
};

/*! Negative notch predicate.

Use intersection (& operator) of another predicate with notInNotch to create notched solid.


		
		geometry explanation:
		
			c: the center
			normalHalfHt (in constructor): A-C
			inside: perpendicular to notch edge, points inside the notch (unit vector)
			normal: perpendicular to inside, perpendicular to both notch planes
			edge: unit vector in the direction of the edge

		          ↑ distUp        A
		-------------------------
		                        | C
		         inside(unit) ← * → distInPlane
		                        |
		-------------------------
		          ↓ distDown      B

*/
class notInNotch: public Predicate{
	Vector3r c, edge, normal, inside; Real aperture;
public:
	notInNotch(const Vector3r& _c, const Vector3r& _edge, const Vector3r& _normal, Real _aperture){
		c=_c;
		edge=_edge; edge.normalize();
		normal=_normal; normal-=edge*edge.dot(normal); normal.normalize();
		inside=edge.cross(normal);
		aperture=_aperture;
		// LOG_DEBUG("edge={}, normal={}, inside={}, aperture={}",edge,normal,inside,aperture);
	}
	bool operator()(const Vector3r& pt, Real pad=0.) const {
		Real distUp=normal.dot(pt-c)-aperture/2, distDown=-normal.dot(pt-c)-aperture/2, distInPlane=-inside.dot(pt-c);
		// LOG_DEBUG("pt={}, distUp={}, distDown={}, distInPlane={}",pt,distUp,distDown,distInPlane);
		if(distInPlane>=pad) return true;
		if(distUp     >=pad) return true;
		if(distDown   >=pad) return true;
		if(distInPlane<0) return false;
		if(distUp  >0) return sqrt(pow2(distInPlane)+pow2(distUp))>=pad;
		if(distDown>0) return sqrt(pow2(distInPlane)+pow2(distUp))>=pad;
		// between both notch planes, closer to the edge than pad (distInPlane<pad)
		return false;
	}
	// This predicate is not bounded, return infinities
	AlignedBox3r aabb() const {
		return AlignedBox3r(Vector3r(-Inf,-Inf,-Inf),Vector3r(Inf,Inf,Inf));
	}
};

// this is only activated in the SCons build
#if defined(WOO_GTS)
// HACK
#include"../src/lib/pygts-0.3.1/pygts.h"

/* Helper function for inGtsSurface::aabb() */
static void vertex_aabb(GtsVertex *vertex, pair<Vector3r,Vector3r> *bb)
{
	GtsPoint *_p=GTS_POINT(vertex);
	Vector3r p(_p->x,_p->y,_p->z);
	bb->first=bb->first.array().min(p.array()).matrix();
	bb->second=bb->second.array().max(p.array()).matrix();
}

/*
This class plays tricks getting around pyGTS to get GTS objects and cache bb tree to speed
up point inclusion tests. For this reason, we have to link with _gts.so (see corresponding
SConscript file), which is at the same time the python module.
*/
class inGtsSurface: public Predicate{
	py::object pySurf; // to hold the reference so that surf is valid
	GtsSurface *surf;
	bool is_open, noPad, noPadWarned;
	GNode* tree;
public:
	inGtsSurface(py::object _surf, bool _noPad=false): pySurf(_surf), noPad(_noPad), noPadWarned(false) {
		if(!pygts_surface_check(_surf.ptr())) throw invalid_argument("Ctor must receive a gts.Surface() instance."); 
		surf=PYGTS_SURFACE_AS_GTS_SURFACE(PYGTS_SURFACE(_surf.ptr()));
	 	if(!gts_surface_is_closed(surf)) throw invalid_argument("Surface is not closed.");
		is_open=gts_surface_volume(surf)<0.;
		if((tree=gts_bb_tree_surface(surf))==NULL) throw runtime_error("Could not create GTree.");
	}
	~inGtsSurface(){g_node_destroy(tree);}
	AlignedBox3r aabb() const override {
		Real inf=std::numeric_limits<Real>::infinity();
		pair<Vector3r,Vector3r> bb; bb.first=Vector3r(inf,inf,inf); bb.second=Vector3r(-inf,-inf,-inf);
		gts_surface_foreach_vertex(surf,(GtsFunc)vertex_aabb,&bb);
		return AlignedBox3r(bb.first,bb.second);
	}
	bool ptCheck(const Vector3r& pt) const{
		GtsPoint gp; gp.x=pt[0]; gp.y=pt[1]; gp.z=pt[2];
		return (bool)gts_point_is_inside_surface(&gp,tree,is_open);
	}
	bool operator()(const Vector3r& pt, Real pad=0.) const override {
		if(noPad){
			if(pad!=0. && noPadWarned) LOG_WARN("inGtsSurface constructed with noPad; requested non-zero pad set to zero.");
			return ptCheck(pt);
		}
		return ptCheck(pt) && ptCheck(pt-Vector3r(pad,0,0)) && ptCheck(pt+Vector3r(pad,0,0)) && ptCheck(pt-Vector3r(0,pad,0))&& ptCheck(pt+Vector3r(0,pad,0)) && ptCheck(pt-Vector3r(0,0,pad))&& ptCheck(pt+Vector3r(0,0,pad));
	}
	py::object surface() const {return pySurf; }
};

#endif

WOO_PYTHON_MODULE(_packPredicates);

PYBIND11_MODULE(_packPredicates,mod){
	WOO_SET_DOCSTRING_OPTS;
	mod.doc()="Spatial predicates for volumes (defined analytically or by triangulation).";
	// base predicate class
	py::class_<Predicate,/*holder*/shared_ptr<Predicate>,/*tramponline*/PredicateWrap>(mod,"Predicate")
		.def(py::init<>()) // docs says this needs to be added as well: https://pybind11.readthedocs.io/en/stable/advanced/classes.html#overriding-virtual-functions-in-python
		.def("__call__",&Predicate::operator(),WOO_PY_ARGS(py::arg("pt"),py::arg("pad")=0.))
		.def("aabb",&Predicate::aabb)
		.def("dim",&Predicate::dim)
		.def("center",&Predicate::center)
		.def("__or__",makeUnion).def("__and__",makeIntersection).def("__sub__",makeDifference).def("__xor__",makeSymmetricDifference);
	// boolean operations
	py::class_<PredicateBoolean,Predicate,shared_ptr<PredicateBoolean>>(mod,"PredicateBoolean","Boolean operation on 2 predicates (abstract class)")
		.def_property_readonly("A",&PredicateBoolean::getA).def_property_readonly("B",&PredicateBoolean::getB);
	py::class_<PredicateUnion,PredicateBoolean,shared_ptr<PredicateUnion>>(mod,"PredicateUnion","Union (non-exclusive disjunction) of 2 predicates. A point has to be inside any of the two predicates to be inside. Can be constructed using the ``|`` operator on predicates: ``pred1 | pred2``.").def(py::init<shared_ptr<Predicate>,shared_ptr<Predicate>>(),py::arg("pred1"),py::arg("pred2"));
	py::class_<PredicateIntersection,PredicateBoolean,shared_ptr<PredicateIntersection>>(mod,"PredicateIntersection","Intersection (conjunction) of 2 predicates. A point has to be inside both predicates. Can be constructed using the ``&`` operator on predicates: ``pred1 & pred2``.").def(py::init<shared_ptr<Predicate>,shared_ptr<Predicate>>(),py::arg("pred1"),py::arg("pred2"));
	py::class_<PredicateDifference,PredicateBoolean,shared_ptr<PredicateDifference>>(mod,"PredicateDifference","Difference (conjunction with negative predicate) of 2 predicates. A point has to be inside the first and outside the second predicate. Can be constructed using the ``-`` operator on predicates: ``pred1 - pred2``.").def(py::init<shared_ptr<Predicate>,shared_ptr<Predicate>>(),py::arg("pred1"),py::arg("pred2"));
	py::class_<PredicateSymmetricDifference,PredicateBoolean,shared_ptr<PredicateSymmetricDifference>>(mod,"PredicateSymmetricDifference","SymmetricDifference (exclusive disjunction) of 2 predicates. A point has to be in exactly one predicate of the two. Can be constructed using the ``^`` operator on predicates: ``pred1 ^ pred2``.").def(py::init<shared_ptr<Predicate>,shared_ptr<Predicate>>(),py::arg("pred1"),py::arg("pred2"));
	// primitive predicates
	py::class_<inSphere,shared_ptr<inSphere>,Predicate>(mod,"inSphere","Sphere predicate.").def(py::init<const Vector3r&,Real>(),py::arg("center"),py::arg("radius"),"Ctor taking center and radius");
	py::class_<inAlignedBox,shared_ptr<inAlignedBox>,Predicate>(mod,"inAlignedBox","Axis-aligned box predicate").def(py::init<const Vector3r&,const Vector3r&>(),py::arg("minCorner"),py::arg("maxCorner"),"Ctor taking minumum and maximum points of the box.").def(py::init<const AlignedBox3r&>(),py::arg("box"));
	py::class_<inOrientedBox,shared_ptr<inOrientedBox>,Predicate>(mod,"inOrientedBox","Arbitrarily oriented box specified as local coordinates (pos,ori) and aligned box in those local coordinates.").def(py::init<const Vector3r&,const Quaternionr&, const AlignedBox3r&>(),py::arg("pos"),py::arg("ori"),py::arg("box"),"Ctor taking position and orientation of the local system, and aligned box in local coordinates.");
	py::class_<inCylSector,shared_ptr<inCylSector>,Predicate>(mod,"inCylSector","Sector of an arbitrarily oriented cylinder in 3d, limiting cylindrical coordinates :math:`\\rho`, :math:`\\theta`, :math:`z`; all coordinate limits are optional.").def(py::init<const Vector3r&, const Quaternionr&, const Vector2r&, const Vector2r&, const Vector2r&>(),py::arg("pos"),py::arg("ori"),py::arg("rrho")=Vector2r::Zero().eval(),py::arg("ttheta")=Vector2r::Zero().eval(),py::arg("zz")=Vector2r::Zero().eval(),"Ctor taking position and orientation of the local system, and aligned box in local coordinates.");
	py::class_<inAlignedHalfspace,shared_ptr<inAlignedHalfspace>,Predicate>(mod,"inAlignedHalfspace","Half-space given by coordinate at some axis.").def(py::init<int, const Real&, bool>(),py::arg("axis"),py::arg("coord"),py::arg("lower")=true,"Ctor taking axis (0,1,2 for :math:`x`, :math:`y`, :math:`z` respectively), the coordinate along that axis, and whether the *lower* half (or the upper half, if *lower* is false) is considered.");
	py::class_<inAxisRange,shared_ptr<inAxisRange>,Predicate>(mod,"inAxisRange","Range of coordinate along some axis, effectively defining two axis-aligned enclosing planes.").def(py::init<int, const Vector2r&>(),py::arg("axis"),py::arg("range"),"Ctor taking axis (0,1,2 for :math:`x`, :math:`y`, :math:`z` respectively), and coordinate range along that axis.");
	py::class_<inParallelepiped,shared_ptr<inParallelepiped>,Predicate>(mod,"inParallelepiped","Parallelepiped predicate").def(py::init<const Vector3r&,const Vector3r&, const Vector3r&, const Vector3r&>(),py::arg("o"),py::arg("a"),py::arg("b"),py::arg("c"),"Ctor taking four points: ``o`` (for origin) and then ``a``, ``b``, ``c`` which define endpoints of 3 respective edges from ``o``.");
	py::class_<inCylinder,shared_ptr<inCylinder>,Predicate>(mod,"inCylinder","Cylinder predicate").def(py::init<const Vector3r&,const Vector3r&,Real>(),py::arg("centerBottom"),py::arg("centerTop"),py::arg("radius"),"Ctor taking centers of the lateral walls and radius.").def("__str__",&inCylinder::__str__);
	py::class_<inHyperboloid,shared_ptr<inHyperboloid>,Predicate>(mod,"inHyperboloid","Hyperboloid predicate").def(py::init<const Vector3r&,const Vector3r&,Real,Real>(),py::arg("centerBottom"),py::arg("centerTop"),py::arg("radius"),py::arg("skirt"),"Ctor taking centers of the lateral walls, radius at bases and skirt (middle radius).");
	py::class_<inEllipsoid,shared_ptr<inEllipsoid>,Predicate>(mod,"inEllipsoid","Ellipsoid predicate").def(py::init<const Vector3r&,const Vector3r&>(),py::arg("centerPoint"),py::arg("abc"),"Ctor taking center of the ellipsoid and its 3 radii.");
	py::class_<notInNotch,shared_ptr<notInNotch>,Predicate>(mod,"notInNotch","Outside of infinite, rectangle-shaped notch predicate").def(py::init<const Vector3r&,const Vector3r&,const Vector3r&,Real>(),py::arg("centerPoint"),py::arg("edge"),py::arg("normal"),py::arg("aperture"),"Ctor taking point in the symmetry plane, vector pointing along the edge, plane normal and aperture size.\nThe side inside the notch is edge×normal.\nNormal is made perpendicular to the edge.\nAll vectors are normalized at construction time."); 
	#ifdef WOO_GTS
		py::class_<inGtsSurface,shared_ptr<inGtsSurface>,Predicate>(mod,"inGtsSurface","GTS surface predicate").def(py::init<py::object,bool>(),py::arg("surface"),py::arg("noPad")=false,"Ctor taking a gts.Surface() instance, which must not be modified during instance lifetime.\nThe optional noPad can disable padding (if set to True), which speeds up calls several times.\nNote: padding checks inclusion of 6 points along +- cardinal directions in the pad distance from given point, which is not exact.")
			.def_property_readonly("surf",&inGtsSurface::surface,"The associated gts.Surface object.");
	#endif
};
