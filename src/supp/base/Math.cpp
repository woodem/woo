#include"Math.hpp"

template<> int ZeroInitializer<int>(){ return (int)0; }
template<> Real ZeroInitializer<Real>(){ return (Real)0; }
template<> unsigned long long ZeroInitializer<unsigned long long>(){ return (unsigned long long)0; }
template<> long ZeroInitializer<long>(){ return (long)0; }

bool MatrixXr_pseudoInverse(const MatrixXr &a, MatrixXr &a_pinv, double epsilon){

	// see : http://en.wikipedia.org/wiki/Moore-Penrose_pseudoinverse#The_general_case_and_the_SVD_method
	if ( a.rows()<a.cols() ) return false;

	// SVD
	Eigen::JacobiSVD<MatrixXr> svdA;
	svdA.compute(a,Eigen::ComputeThinU|Eigen::ComputeThinV);
	MatrixXr vSingular = svdA.singularValues();

	// Build a diagonal matrix with the Inverted Singular values
	// The pseudo inverted singular matrix is easy to compute :
	// is formed by replacing every nonzero entry by its reciprocal (inversing).
	VectorXr vPseudoInvertedSingular(svdA.matrixV().cols(),1);
		
	for (int iRow =0; iRow<vSingular.rows(); iRow++){
		if(fabs(vSingular(iRow))<=epsilon) vPseudoInvertedSingular(iRow,0)=0.;
	   else vPseudoInvertedSingular(iRow,0)=1./vSingular(iRow);
	}

	// A little optimization here 
	MatrixXr mAdjointU = svdA.matrixU().adjoint().block(0,0,vSingular.rows(),svdA.matrixU().adjoint().cols());

	// Pseudo-Inversion : V * S * U'
	a_pinv = (svdA.matrixV() *  vPseudoInvertedSingular.asDiagonal()) * mAdjointU;

	return true;
}

