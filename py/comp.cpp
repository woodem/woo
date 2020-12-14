
#include<woo/lib/base/CompUtils.hpp>
#include<woo/lib/base/Volumetric.hpp>
#include<woo/core/Master.hpp> // for WOO_PYTHON_MODULE
#include<woo/lib/smoothing/LinearInterpolate.hpp>


py::tuple computePrincipalAxes(const Real& V, const Vector3r& Sg, const Matrix3r& Ig){
	if(V<0) throw std::invalid_argument("Volume V must be greater than 0 (is vertex ordering inverted?).");
	Vector3r pos; Quaternionr ori; Vector3r inertia;
	woo::Volumetric::computePrincipalAxes(V,Sg,Ig,pos,ori,inertia);
	return py::make_tuple(pos,ori,inertia);
};

Matrix3r tetraInertia_cov(const Vector3r& A, const Vector3r& B, const Vector3r& C, const Vector3r& D, bool fixSign){
	Vector3r v[]={A,B,C,D};
	Real vol=0; // discarded
	return woo::Volumetric::tetraInertia_cov(v,vol);
}

Matrix3r tetraInertia_grid(const Vector3r& A, const Vector3r& B, const Vector3r& C, const Vector3r& D,int div){
	Vector3r v[]={A,B,C,D};
	return woo::Volumetric::tetraInertia_grid(v,div);
}

vector<Real> uniformResamplePiecewiseLinear(const vector<Real>& xx, const vector<Real>& yy, const Vector2r& xRange, int div){
	vector<Real> ret; ret.resize(div);
	size_t pos=0;
	Real step=(xRange[1]-xRange[0])/(div-1.);
	for(int i=0; i<div; i++){
		ret[i]=linearInterpolate(xRange[0]+i*step,xx,yy,pos);
	}
	return ret;
}

WOO_PYTHON_MODULE(comp);
PYBIND11_MODULE(comp,mod){
	WOO_SET_DOCSTRING_OPTS;
	mod.doc()="Expose interal utility c++ functions for computing volume, centroids, inertia, coordinate transforms; mostly used for testing.";
	mod.def("tetraVolume",&woo::Volumetric::tetraVolume,WOO_PY_ARGS(py::arg("A"),py::arg("B"),py::arg("C"),py::arg("D")),"Volume of tetrahedron, computed as :math:`\\frac{1}{6}((A-B)\\cdot(B-D)))\\times(C-D)`.");
	// mod.def("tetraInertia",&woo::Volumetric::tetraInertia,WOO_PY_ARGS(py::arg("A"),py::arg("B"),py::arg("C"),py::arg("D")),"Inertia tensor of tetrahedron given its vertex coordinates; the algorithm is described in :cite:`Tonon2005`.");
	mod.def("tetraInertia",tetraInertia_cov,WOO_PY_ARGS(py::arg("A"),py::arg("B"),py::arg("C"),py::arg("D"),py::arg("fixSign")=true),"Tetrahedron inertia from covariance. If *fixSign* is true, the tensor is multiplied by -1 if the (0,0) entry is negative (this is caued by non-canonical vertex ordering).");
	mod.def("tetraInertia_grid",tetraInertia_grid,WOO_PY_ARGS(py::arg("A"),py::arg("B"),py::arg("C"),py::arg("D"),py::arg("div")=100),"Tetrahedron inertia from grid sampling (*div* gives subdivision along the shortest aabb side).");

	mod.def("triangleArea",&woo::Volumetric::triangleArea);
	mod.def("triangleInertia",&woo::Volumetric::triangleInertia);
	mod.def("inertiaTensorTranslate",&woo::Volumetric::inertiaTensorTranslate,WOO_PY_ARGS(py::arg("I"),py::arg("V"),py::arg("off")),"Tensor of inertia of solid translated by *off* with previous inertia :math:`I`, volume :math:`V`; if :math:`V` is positive, the translation is away from the centroid, if negative, towards the centroid. The computation implements `Parallel axes theorem <https://en.wikipedia.org/wiki/Parallel_axis_theorem>`__.");
	mod.def("computePrincipalAxes",computePrincipalAxes,WOO_PY_ARGS(py::arg("V"),py::arg("Sg"),py::arg("Ig")),"Return (*pos*, *ori*, *inertia*) of new coordinate system (relative to the current one), whose axes are principal, i.e. second-order momentum is diagonal (the diagonal is returned in *inertia*, which is sorted to be non-decreasing). The arguments are volume (mass) *V*, first-order momentum *Sg* and second-order momentum *Ig*. If *Sg* is ``(0,0,0)``, the reference point will not change, only rotation will occur.");
	mod.def("cart2cyl",&CompUtils::cart2cyl,WOO_PY_ARGS(py::arg("xyz")),"Convert cartesian coordinates to cylindrical; cylindrical coordinates are specified as :math:`(r,\\theta,z)`, the reference plane is the :math:`xy`-plane (see `at Wikipedia <http://en.wikipedia.org/wiki/Cylindrical_coordinates>`__).");
	mod.def("cyl2cart",&CompUtils::cyl2cart,WOO_PY_ARGS(py::arg("rThetaZ")),"Convert cylindrical coordinates to cartesian; cylindrical coordinates are specified as :math:`(r,\\theta,z)`, the reference plane is the :math:`xy`-plane (see `at Wikipedia <http://en.wikipedia.org/wiki/Cylindrical_coordinates>`__).");
	mod.def("uniformResamplePiecewiseLinear",uniformResamplePiecewiseLinear,WOO_PY_ARGS(py::arg("xx"),py::arg("yy"),py::arg("xRange"),py::arg("div")),"Resample piecewise-linear function (given by points *xx* and *yy*, where *xx* is non-decreasing) using a uniform grid with endpoints *xRange* and *div* points (*div*-1 pieces), returning linearly interpolated value of *yy* in *div*+1 points.");
};
