#pragma once

#include"Particle.hpp"
#include"../core/Engine.hpp"

#include"../supp/sphere-pack/SpherePack.hpp"

struct AnisoPorosityAnalyzer: public Engine {
	bool acceptsField(Field* f) override { return dynamic_cast<DemField*>(f); }
	DemField* dem;
	SpherePack pack;
	virtual void run() override;
	static vector<Vector3r> splitRay(Real theta, Real phi, Vector3r pt0=Vector3r::Zero(), const Matrix3r& T=Matrix3r::Identity());
	Real relSolid(Real theta, Real phi, Vector3r pt0=Vector3r::Zero(), bool vis=false);
	// _check variants to be called from python (safe scene setup etc)
	Real computeOneRay_check(const Vector3r& A, const Vector3r& B, bool vis=true);
	Real computeOneRay_angles_check(Real theta, Real phi, bool vis=true);
	void clearVis(){ rayIds.clear(); rayPts.clear(); }

	Real computeOneRay(const Vector3r& A, const Vector3r& B, bool vis=false);
	void initialize();
	WOO_DECL_LOGGER;
	#define woo_dem_AnisoPorosityAnalyzer__CLASS_BASE_DOC_ATTRS_PY \
		AnisoPorosityAnalyzer,Engine,"Engine which analyzes current scene and computes directionaly porosity value by intersecting spheres with lines. The algorithm only works on periodic simulations.", \
		((Matrix3r,poro,Matrix3r::Zero(),AttrTrait<Attr::readonly>(),"Store analysis result here")) \
		((int,div,10,,"Fineness of division of interval (0…1) for $u$,$v$ ∈〈0…1〉, which are used for uniform distribution over the positive octant as $\\theta=\frac{\\pi}{2}u$, $\\phi=\\arccos v$ (see http://mathworld.wolfram.com/SpherePointPicking.html)")) \
		/* check that data are up-to-date */ \
		((long,initStep,-1,AttrTrait<Attr::hidden>(),"Step in which internal data were last updated")) \
		((size_t,initNum,-1,AttrTrait<Attr::hidden>(),"Number of particles at last update")) \
		((vector<Particle::id_t>,rayIds,,AttrTrait<Attr::readonly>(),"Particles intersected with ray when *oneRay* was last called from python.")) \
		((vector<Vector3r>,rayPts,,AttrTrait<Attr::readonly>(),"Starting and ending points of segments intersecting particles.")) \
		,/*py*/ \
			.def("oneRay",&AnisoPorosityAnalyzer::computeOneRay_check,WOO_PY_ARGS(py::arg("A"),py::arg("B")=Vector3r(Vector3r::Zero()),py::arg("vis")=true)) \
			.def("oneRay",&AnisoPorosityAnalyzer::computeOneRay_angles_check,WOO_PY_ARGS(py::arg("theta"),py::arg("phi"),py::arg("vis")=true)) \
			.def_static("splitRay",&AnisoPorosityAnalyzer::splitRay,WOO_PY_ARGS(py::arg("theta"),py::arg("phi"),py::arg("pt0")=Vector3r::Zero().eval(),py::arg("T")=Matrix3r::Identity().eval())) \
			.def("relSolid",&AnisoPorosityAnalyzer::relSolid,WOO_PY_ARGS(py::arg("theta"),py::arg("phi"),py::arg("pt0")=Vector3r::Zero().eval(),py::arg("vis")=false)) \
			.def("clearVis",&AnisoPorosityAnalyzer::clearVis,"Clear visualizable intersection segments")
	WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_AnisoPorosityAnalyzer__CLASS_BASE_DOC_ATTRS_PY);
};
WOO_REGISTER_OBJECT(AnisoPorosityAnalyzer);


#ifdef WOO_OPENGL
#include"../gl/Renderer.hpp"

class GlExtra_AnisoPorosityAnalyzer: public GlExtraDrawer{
	public:
	WOO_DECL_LOGGER;
	virtual void render(const GLViewInfo&) override;
	Real idColor(int id){ return (id%idMod)*1./(idMod-1); }
	#define woo_gl_GlExtra_AnisoPorosityAnalyzer__CLASS_BASE_DOC_ATTRS \
		GlExtra_AnisoPorosityAnalyzer,GlExtraDrawer,"Find an instance of :obj:`LawTester` and show visually its data.", \
		((shared_ptr<AnisoPorosityAnalyzer>,analyzer,,AttrTrait<>(),"Associated :obj:`AnisoPorosityAnalyzer` object.")) \
		((int,wd,2,,"Segment line width")) \
		((Vector2i,wd_range,Vector2i(1,10),AttrTrait<>().noGui(),"Range for wd")) \
		((int,num,2,,"Number to show at the segment middle: 0 = nothing, 1 = particle id, 2 = intersected length")) \
		((Vector2i,num_range,Vector2i(0,2),AttrTrait<>().noGui(),"Range for num")) \
		((int,idMod,5,,"Modulate particle id by this number to get segment color"))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_gl_GlExtra_AnisoPorosityAnalyzer__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(GlExtra_AnisoPorosityAnalyzer);
#endif
