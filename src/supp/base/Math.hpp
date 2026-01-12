// © 2010 Václav Šmilauer <eudoxos@arcig.cz>
#pragma once

#if 0 // broken, do not use
// optimize as much as possible even in the debug mode (effective?)
#if defined(__GNUG__) && __GNUC__ >= 4 && __GNUC_MINOR__ >=4
	#pragma GCC push_options
	#pragma GCC optimize "2"
#endif
#endif

#ifdef QUAD_PRECISION
	typedef long double quad;
	typedef quad Real;
#else
	typedef double Real;
#endif

#include<limits>
#include<cstdlib>
#include<vector>

#include"Types.hpp"


// BEGIN workaround for
// * http://eigen.tuxfamily.org/bz/show_bug.cgi?id=528
// * https://sourceforge.net/tracker/index.php?func=detail&aid=3584127&group_id=202880&atid=983354
// (only needed with gcc <= 4.7)
// must come before Eigen/Core is included
#include<stdlib.h>
#include<sys/stat.h>
// END workaround

/* clang 3.3 warns:
		/usr/include/eigen3/Eigen/src/Core/products/SelfadjointMatrixVector.h:82:5: warning: 'register' storage class specifier is deprecated [-Wdeprecated]
			register const Scalar* __restrict A0 = lhs + j*lhsStride;
	we silence this warning with diagnostic pragmas:
*/
#if defined(__GNUC__) || defined(__clang__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated"
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#define EIGEN_NO_DEBUG
#include<Eigen/Core>
#include<Eigen/Geometry>
#include<Eigen/Eigenvalues>
#include<Eigen/QR>
#include<Eigen/LU>
#include<Eigen/SVD>
#include<float.h>
#ifdef WOO_ALIGN
	#include<unsupported/Eigen/AlignedVector3>
#endif


#if defined(__GNUC__) || defined(__clang__)
	#pragma GCC diagnostic pop
#endif

// mimick expectation macros that linux has (see e.g. http://kerneltrap.org/node/4705)
#define WOO_LIKELY(x) __builtin_expect(!!(x),1)
#define WOO_UNLIKELY(x) __builtin_expect(!!(x),0)


#if 1
	template<typename Scalar> using Vector2_=Eigen::Matrix<Scalar,2,1>;
	template<typename Scalar> using Vector3_=Eigen::Matrix<Scalar,3,1>;
	template<typename Scalar> using Vector4_=Eigen::Matrix<Scalar,4,1>;
	template<typename Scalar> using Vector6_=Eigen::Matrix<Scalar,6,1>;
	template<typename Scalar> using Matrix3_=Eigen::Matrix<Scalar,3,3>;
	template<typename Scalar> using Matrix6_=Eigen::Matrix<Scalar,6,6>;
	typedef Vector2_<int> Vector2i;
	typedef Vector2_<Real> Vector2r;
	typedef Vector3_<int> Vector3i;
	#ifdef WOO_ALIGN
		typedef Eigen::AlignedVector3<Real> Vector3r;
	#else
		typedef Vector3_<Real> Vector3r;
	#endif
	typedef Vector4_<Real> Vector4r; // never used
	typedef Vector6_<int> Vector6i;
	typedef Vector6_<Real> Vector6r;
	typedef Vector3_<Real> Vector3r;
	typedef Matrix3_<Real> Matrix3r;
	typedef Matrix3_<int> Matrix3i;
	typedef Matrix6_<Real> Matrix6r;
#else
	// templates of those types with single parameter are not possible, use macros for now
	#define Vector2_<Scalar> Eigen::Matrix<Scalar,2,1>
	#define Vector3_<Scalar> Eigen::Matrix<Scalar,3,1>
	#define Vector4_<Scalar> Eigen::Matrix<Scalar,4,1>
	#define Vector6_<Scalar> Eigen::Matrix<Scalar,6,1>
	#define Matrix3_<Scalar> Eigen::Matrix<Scalar,3,3>
	#define Matrix6_<Scalar> Eigen::Matrix<Scalar,6,6>

	#if 0
		// etc
	#endif

	typedef VECTOR2_TEMPLATE(int) Vector2i;
	typedef VECTOR2_TEMPLATE(Real) Vector2r;
	typedef VECTOR3_TEMPLATE(int) Vector3i;
	#ifndef WOO_ALIGN
		typedef VECTOR3_TEMPLATE(Real) Vector3r;
	#else
		typedef Eigen::AlignedVector3<Real> Vector3r;
	#endif
	typedef VECTOR4_TEMPLATE(Real) Vector4r;
	typedef VECTOR6_TEMPLATE(Real) Vector6r;
	typedef VECTOR6_TEMPLATE(int) Vector6i;
	typedef MATRIX3_TEMPLATE(Real) Matrix3r;
	typedef MATRIX3_TEMPLATE(int) Matrix3i;
	typedef MATRIX6_TEMPLATE(Real) Matrix6r;
#endif

typedef Eigen::Matrix<Real,Eigen::Dynamic,Eigen::Dynamic> MatrixXr;
typedef Eigen::Matrix<Real,Eigen::Dynamic,1> VectorXr;

typedef Eigen::Quaternion<Real> Quaternionr;
typedef Eigen::AngleAxis<Real> AngleAxisr;
typedef Eigen::AlignedBox<Real,2> AlignedBox2r;
typedef Eigen::AlignedBox<Real,3> AlignedBox3r;
typedef Eigen::AlignedBox<int,2> AlignedBox2i;
typedef Eigen::AlignedBox<int,3> AlignedBox3i;
using Eigen::AngleAxis; using Eigen::Quaternion;

// in some cases, we want to initialize types that have no default constructor (OpenMPAccumulator, for instance)
// template specialization will help us here
template<typename EigenMatrix> EigenMatrix ZeroInitializer(){ return EigenMatrix::Zero(); };
template<> int ZeroInitializer<int>();
template<> long ZeroInitializer<long>();
template<> unsigned long long ZeroInitializer<unsigned long long>();
template<> Real ZeroInitializer<Real>();


// io
template<class Scalar> std::ostream & operator<<(std::ostream &os, const Vector2_<Scalar>& v){ os << v.x()<<" "<<v.y(); return os; };
template<class Scalar> std::ostream & operator<<(std::ostream &os, const Vector3_<Scalar>& v){ os << v.x()<<" "<<v.y()<<" "<<v.z(); return os; };
template<class Scalar> std::ostream & operator<<(std::ostream &os, const Vector4_<Scalar>& v){ os << v.x()<<" "<<v.y()<<" "<<v.z(); return os; };
template<class Scalar> std::ostream & operator<<(std::ostream &os, const Vector6_<Scalar>& v){ os << v[0]<<" "<<v[1]<<" "<<v[2]<<" "<<v[3]<<" "<<v[4]<<" "<<v[5]; return os; };
template<class Scalar> std::ostream & operator<<(std::ostream &os, const Eigen::Quaternion<Scalar>& q){ os<<q.w()<<" "<<q.x()<<" "<<q.y()<<" "<<q.z(); return os; };
// operators
//template<class Scalar> Vector3_<Scalar> operator*(Scalar s, const Vector3_<Scalar>& v) {return v*s;}
//template<class Scalar> Matrix3_<Scalar> operator*(Scalar s, const Matrix3_<Scalar>& m) { return m*s; }
//template<class Scalar> Quaternion<Scalar> operator*(Scalar s, const Quaternion<Scalar>& q) { return q*s; }
template<typename Scalar> void matrixEigenDecomposition(const Matrix3_<Scalar> m, Matrix3_<Scalar>& mRot, Matrix3_<Scalar>& mDiag){ Eigen::SelfAdjointEigenSolver<Matrix3_<Scalar>> a(m); mRot=a.eigenvectors(); mDiag=a.eigenvalues().asDiagonal(); }
// http://eigen.tuxfamily.org/dox/TutorialGeometry.html
template<typename Scalar> Matrix3_<Scalar> matrixFromEulerAnglesXYZ(Scalar x, Scalar y, Scalar z){ Matrix3_<Scalar> m; m=AngleAxis<Scalar>(x,Vector3_<Scalar>::UnitX())*AngleAxis<Scalar>(y,Vector3_<Scalar>::UnitY())*AngleAxis<Scalar>(z,Vector3_<Scalar>::UnitZ()); return m;}
template<typename Scalar> bool operator==(const Quaternion<Scalar>& u, const Quaternion<Scalar>& v){ return u.x()==v.x() && u.y()==v.y() && u.z()==v.z() && u.w()==v.w(); }
template<typename Scalar> bool operator!=(const Quaternion<Scalar>& u, const Quaternion<Scalar>& v){ return !(u==v); }
template<typename Scalar> bool operator==(const Matrix3_<Scalar>& m, const Matrix3_<Scalar>& n){ for(int i=0;i<3;i++)for(int j=0;j<3;j++)if(m(i,j)!=n(i,j)) return false; return true; }
template<typename Scalar> bool operator!=(const Matrix3_<Scalar>& m, const Matrix3_<Scalar>& n){ return !(m==n); }
template<typename Scalar> bool operator==(const Matrix6_<Scalar>& m, const Matrix6_<Scalar>& n){ for(int i=0;i<6;i++)for(int j=0;j<6;j++)if(m(i,j)!=n(i,j)) return false; return true; }
template<typename Scalar> bool operator!=(const Matrix6_<Scalar>& m, const Matrix6_<Scalar>& n){ return !(m==n); }
template<typename Scalar> bool operator==(const Vector6_<Scalar>& u, const Vector6_<Scalar>& v){ return u[0]==v[0] && u[1]==v[1] && u[2]==v[2] && u[3]==v[3] && u[4]==v[4] && u[5]==v[5]; }
template<typename Scalar> bool operator!=(const Vector6_<Scalar>& u, const Vector6_<Scalar>& v){ return !(u==v); }
template<typename Scalar> bool operator==(const Vector4_<Scalar>& u, const Vector4_<Scalar>& v){ return u[0]==v[0] && u[1]==v[1] && u[2]==v[2] && u[3]=v[3]; }
template<typename Scalar> bool operator!=(const Vector4_<Scalar>& u, const Vector4_<Scalar>& v){ return !(u==v); }
template<typename Scalar> bool operator==(const Vector3_<Scalar>& u, const Vector3_<Scalar>& v){ return u.x()==v.x() && u.y()==v.y() && u.z()==v.z(); }
template<typename Scalar> bool operator!=(const Vector3_<Scalar>& u, const Vector3_<Scalar>& v){ return !(u==v); }
template<typename Scalar> bool operator==(const Vector2_<Scalar>& u, const Vector2_<Scalar>& v){ return u.x()==v.x() && u.y()==v.y(); }
template<typename Scalar> bool operator!=(const Vector2_<Scalar>& u, const Vector2_<Scalar>& v){ return !(u==v); }
template<typename Scalar> Quaternion<Scalar> operator*(Scalar f, const Quaternion<Scalar>& q){ return Quaternion<Scalar>(q.coeffs()*f); }
template<typename Scalar> Quaternion<Scalar> operator+(Quaternion<Scalar> q1, const Quaternion<Scalar>& q2){ return Quaternion<Scalar>(q1.coeffs()+q2.coeffs()); }	/* replace all those by standard math functions
	this is a non-templated version, to avoid compilation because of static constants;
*/
template<typename Scalar>
struct Math{
	static Scalar Sign(Scalar f){ if(f<0) return -1; if(f>0) return 1; return 0; }
	static Scalar UnitRandom(){ return ((double)rand()/((double)(RAND_MAX))); }
	static Eigen::Matrix<Scalar,3,1> UnitRandom3(){ return Eigen::Matrix<Scalar,3,1>(UnitRandom(),UnitRandom(),UnitRandom()); }
	static Scalar IntervalRandom(const Real& a, const Real& b){ return a+UnitRandom()*(b-a); }
	static Scalar SymmetricRandom(){ return 2.*(((double)rand())/((double)(RAND_MAX)))-1.; }
	static Scalar FastInvCos0(Scalar fValue){ Scalar fRoot = sqrt(((Scalar)1.0)-fValue); Scalar fResult = -(Scalar)0.0187293; fResult *= fValue; fResult += (Scalar)0.0742610; fResult *= fValue; fResult -= (Scalar)0.2121144; fResult *= fValue; fResult += (Scalar)1.5707288; fResult *= fRoot; return fResult; }
	// see http://planning.cs.uiuc.edu/node198.html (uses inverse notation: x,y,z,w!!
	// and also http://www.mech.utah.edu/~brannon/public/rotation.pdf pg 110
	static Quaternion<Scalar> UniformRandomRotation(){ Scalar u1=UnitRandom(), u2=UnitRandom(), u3=UnitRandom(); return Quaternion<Scalar>(/*w*/sqrt(u1)*cos(2*M_PI*u3),/*x*/sqrt(1-u1)*sin(2*M_PI*u2),/*y*/sqrt(1-u1)*cos(2*M_PI*u2),/*z*/sqrt(u1)*sin(2*M_PI*u3)); }
};
typedef Math<Real> Mathr;

// http://stackoverflow.com/questions/1903954/is-there-a-standard-sign-function-signum-sgn-in-c-c
template <typename T> int sgn(T val){ return (val>T(0))-(val<T(0)); }


template<typename T> T pow2(T x){ return x*x; }
template<typename T> T pow3(T x){ return x*x*x; }
template<typename T> T pow4(T x){ return pow2(x*x); }
template<typename T> T pow5(T x){ return pow4(x)*x; }
// https://stackoverflow.com/a/13771544/761090
// Computes x^n, where n is a natural number.
template<typename T>
T pown(T x, unsigned n){
	//double y = 1;
	// n = 2*d + r. x^n = (x^2)^d * x^r.
	unsigned d = n >> 1;
	unsigned r = n & 1;
	T x_2_d = d == 0? 1 : pown(x*x, d);
	T x_r = r == 0? 1 : x;
	return x_2_d*x_r;
}


/* this was removed in eigen3, see http://forum.kde.org/viewtopic.php?f=74&t=90914 */
template<typename MatrixT>
void Matrix_computeUnitaryPositive(const MatrixT& in, MatrixT* unitary, MatrixT* positive){
	assert(unitary); assert(positive); 
	Eigen::JacobiSVD<MatrixT> svd(in, Eigen::ComputeThinU | Eigen::ComputeThinV);
	MatrixT mU, mV, mS;
	mU = svd.matrixU();
   mV = svd.matrixV();
   mS = svd.singularValues().asDiagonal();
	*unitary=mU * mV.adjoint();
	*positive=mV * mS * mV.adjoint();
}


bool MatrixXr_pseudoInverse(const MatrixXr &a, MatrixXr &a_pinv, double epsilon=std::numeric_limits<MatrixXr::Scalar>::epsilon());



/*
 * Extra woo math functions and classes
 */


/* convert Vector6r in the Voigt notation to corresponding 2nd order symmetric tensor (stored as Matrix3r)
	if strain is true, then multiply non-diagonal parts by .5
*/
template<typename Scalar>
Matrix3_<Scalar> voigt_toSymmTensor(const Vector6_<Scalar>& v, bool strain=false){
	Real k=(strain?.5:1.);
	Matrix3_<Scalar> ret; ret<<v[0],k*v[5],k*v[4], k*v[5],v[1],k*v[3], k*v[4],k*v[3],v[2]; return ret;
}
/* convert 2nd order tensor to 6-vector (Voigt notation), symmetrizing the tensor;
	if strain is true, multiply non-diagonal compoennts by 2.
*/
template<typename Scalar>
Vector6_<Scalar> tensor_toVoigt(const Matrix3_<Scalar>& m, bool strain=false){
	int k=(strain?2:1);
	Vector6_<Scalar> ret; ret<<m(0,0),m(1,1),m(2,2),k*.5*(m(1,2)+m(2,1)),k*.5*(m(2,0)+m(0,2)),k*.5*(m(0,1)+m(1,0)); return ret;
}

/* Apply Levi-Civita permutation tensor on m
	http://en.wikipedia.org/wiki/Levi-Civita_symbol
*/
template<typename Scalar>
Vector3_<Scalar> leviCivita(const Matrix3_<Scalar>& m){
	// i,j,k: v_i=ε_ijk W_j,k
	// +: 1,2,3; 3,1,2; 2,3,1
	// -: 1,3,2; 3,2,1; 2,1,3
	return Vector3r(/*+2,3-3,2*/m(1,2)-m(2,1),/*+3,1-1,3*/m(2,0)-m(0,2),/*+1,2-2,1*/m(0,1)-m(1,0));
}

// mapping between 6x6 matrix indices and tensor indices (Voigt notation)
__attribute__((unused))
const short voigtMap[6][6][4]={
	{{0,0,0,0},{0,0,1,1},{0,0,2,2},{0,0,1,2},{0,0,2,0},{0,0,0,1}},
	{{1,1,0,0},{1,1,1,1},{1,1,2,2},{1,1,1,2},{1,1,2,0},{1,1,0,1}},
	{{2,2,0,0},{2,2,1,1},{2,2,2,2},{2,2,1,2},{2,2,2,0},{2,2,0,1}},
	{{1,2,0,0},{1,2,1,1},{1,2,2,2},{1,2,1,2},{1,2,2,0},{1,2,0,1}},
	{{2,0,0,0},{2,0,1,1},{2,0,2,2},{2,0,1,2},{2,0,2,0},{2,0,0,1}},
	{{0,1,0,0},{0,1,1,1},{0,1,2,2},{0,1,1,2},{0,1,2,0},{0,1,0,1}}
};

__attribute__((unused))
const Real NaN(std::numeric_limits<Real>::signaling_NaN());

__attribute__((unused))
const Real Inf(std::numeric_limits<Real>::infinity());



#include <fmt/ostream.h>

template<typename T, int Rows, int Cols>
struct fmt::formatter<Eigen::Matrix<T,Rows,Cols>> : fmt::ostream_formatter {};
