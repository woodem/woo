// many thanks to http://codesuppository.blogspot.com/2006_06_01_archive.html
// the code written after http://www.amillionpixels.us/bestfitobb.cpp
// which is MIT-licensed

#include"../src/supp/base/Logging.hpp"
#include"../src/supp/base/Math.hpp"
#include"../src/supp/pyutil/doc_opts.hpp"

#include"../src/supp/base/Types.hpp"

#include"../src/core/Master.hpp"

#include<vector>
#include<stdexcept>

// compute minimum bounding for a cloud of points

// returns volume
Real computeOBB(const std::vector<Vector3r>& pts, const Matrix3r& rot, Vector3r& center, Vector3r& halfSize){
	const Real inf=std::numeric_limits<Real>::infinity();
	Vector3r mn(inf,inf,inf), mx(-inf,-inf,-inf);
	for(const Vector3r& pt: pts){
		Vector3r ptT=rot*pt;
		mn=mn.array().min(ptT.array()).matrix(); mx=mx.array().max(ptT.array()).matrix();
	}
	halfSize=.5*(mx-mn);
	center=.5*(mn+mx);
	return 8*halfSize[0]*halfSize[1]*halfSize[2];
}

void bestFitOBB(const std::vector<Vector3r>& pts, Vector3r& center, Vector3r& halfSize, Quaternionr& rot){
	Vector3r angle0(Vector3r::Zero()), angle(Vector3r::Zero());
	Vector3r center0; Vector3r halfSize0;
	Real bestVolume=std::numeric_limits<Real>::infinity();
	Real sweep=M_PI/4; Real steps=7.;
	while(sweep>=M_PI/180.){
		bool found=false;
		Real stepSize=sweep/steps;
		for(Real x=angle0[0]-sweep; x<=angle0[0]+sweep; x+=stepSize){
			for(Real y=angle0[1]-sweep; y<angle0[1]+sweep; y+=stepSize){
				for(Real z=angle0[2]-sweep; z<angle0[2]+sweep; z+=stepSize){
					Matrix3r rot0=matrixFromEulerAnglesXYZ(x,y,z);
					Real volume=computeOBB(pts,rot0,center0,halfSize0);
					if(volume<bestVolume){
						bestVolume=volume;
						angle=angle0;
						// set return values, in case this will be really the best fit
						rot=Quaternionr(rot0); rot=rot.conjugate(); center=center0; halfSize=halfSize0;
						found=true;
					}
				}
			}
		}
		if(found){
			angle0=angle;
			sweep*=.5;
		} else return;
	}
}

py::tuple bestFitOBB_py(const py::tuple& _pts){
	int l=py::len(_pts);
	if(l<=1) throw std::runtime_error("Cloud must have at least 2 points.");
	std::vector<Vector3r> pts; pts.resize(l);
	for(int i=0; i<l; i++) pts[i]=py::extract<Vector3r>(_pts[i]);
	Quaternionr rot; Vector3r halfSize, center;
	bestFitOBB(pts,center,halfSize,rot);
	return py::make_tuple(center,halfSize,rot);
}

WOO_PYTHON_MODULE(_packObb);
PYBIND11_MODULE(_packObb,mod){
	WOO_SET_DOCSTRING_OPTS;
	mod.attr("__name__")="woo._packObb";
	mod.doc()="Computation of oriented bounding box for cloud of points.";
	mod.def("cloudBestFitOBB",bestFitOBB_py,"Return (Vector3 center, Vector3 halfSize, Quaternion orientation) of\nbest-fit oriented bounding-box for given tuple of points\n(uses brute-force velome minimization, do not use for very large clouds).");
};
