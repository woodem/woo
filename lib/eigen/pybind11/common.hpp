#pragma once
#include<woo/lib/base/Math.hpp>
#include<woo/lib/base/Types.hpp>



// #include<unsupported/Eigen/AlignedVector3>
#if 0
// integral type for indices, to avoid compiler warnings with int
typedef Eigen::Matrix<int,1,1>::Index Index;

/* exposed types */
typedef Eigen::Matrix<int ,2,1> Vector2i;
typedef Eigen::Matrix<Real,2,1> Vector2r;
typedef Eigen::Matrix<int ,3,1> Vector3i;
typedef Eigen::Matrix<Real,3,1> Vector3r;
// typedef Eigen::AlignedVector3<Real> Vector3ra;
typedef Eigen::Matrix<Real,4,1> Vector4r;
typedef Eigen::Matrix<int ,6,1> Vector6i;
typedef Eigen::Matrix<Real,6,1> Vector6r;
typedef Eigen::Matrix<Real,3,3> Matrix3r;
typedef Eigen::Matrix<Real,6,6> Matrix6r;

typedef Eigen::Matrix<Real,Eigen::Dynamic,Eigen::Dynamic> MatrixXr;
typedef Eigen::Matrix<Real,Eigen::Dynamic,1> VectorXr;

typedef Eigen::Quaternion<Real> Quaternionr;
typedef Eigen::AngleAxis<Real> AngleAxisr;
typedef Eigen::AlignedBox<Real,3> AlignedBox3r;
typedef Eigen::AlignedBox<Real,2> AlignedBox2r;
#endif

// #define _COMPLEX_SUPPORT

#ifdef _COMPLEX_SUPPORT
#include<complex>
	using std::complex;
	typedef Eigen::Matrix<complex<Real>,2,1> Vector2cr;
	typedef Eigen::Matrix<complex<Real>,3,1> Vector3cr;
	typedef Eigen::Matrix<complex<Real>,6,1> Vector6cr;
	typedef Eigen::Matrix<complex<Real>,Eigen::Dynamic,1> VectorXcr;
	typedef Eigen::Matrix<complex<Real>,3,3> Matrix3cr;
	typedef Eigen::Matrix<complex<Real>,6,6> Matrix6cr;
	typedef Eigen::Matrix<complex<Real>,Eigen::Dynamic,Eigen::Dynamic> MatrixXcr;
#endif


/**** double-conversion helpers *****/
#include<double-conversion/double-conversion.h>

static double_conversion::DoubleToStringConverter doubleToString(
	double_conversion::DoubleToStringConverter::NO_FLAGS,
	"inf", /* infinity symbol */
	"nan", /* NaN symbol */
	'e', /*exponent symbol*/
	-5, /* decimal_in_shortest_low: 0.0001, but 0.00001->1e-5 */
	7, /* decimal_in_shortest_high */
	/* the following are irrelevant for the shortest representation */
	6, /* max_leading_padding_zeroes_in_precision_mode */
	6 /* max_trailing_padding_zeroes_in_precision_mode */
);

/* optionally pad from the left */
static inline string doubleToShortest(double d, int pad=0){
	/* 32 is perhaps wasteful */
	/* it would be better to write to the string's buffer itself, not sure how to do that */
	char buf[32];
	double_conversion::StringBuilder sb(buf,32);
	doubleToString.ToShortest(d,&sb);
	string ret(sb.Finalize());
	if(pad==0 || (int)ret.size()>=pad) return ret;
	return string(pad-ret.size(),' ')+ret; // left-padded if shorter
} 


/* generic function to print numbers, via std::to_string plus padding -- used for ints */
template<typename T>
string num_to_string(const T& num, int pad=0){
	string ret(std::to_string(num));
	if(pad==0 || (int)ret.size()>=pad) return ret;
	return string(pad-ret.size(),' ')+ret; // left-pad with spaces
}

// for doubles, use the shortest representation
static inline string num_to_string(const double& num, int pad=0){ return doubleToShortest(num,pad); }

#ifdef _COMPLEX_SUPPORT
	// for complex numbers (with any scalar type, though only doubles are really used)
	template<typename T>
	string num_to_string(const complex<T>& num, int pad=0){
		string ret;
		// both components non-zero
		if(num.real()!=0 && num.imag()!=0){
			// don't add "+" in the middle if imag is negative and will start with "-"
			string ret=num_to_string(num.real(),/*pad*/0)+(num.imag()>0?"+":"")+num_to_string(num.imag(),/*pad*/0)+"j";
			if(pad==0 || (int)ret.size()>=pad) return ret;
			return string(pad-ret.size(),' ')+ret; // left-pad with spaces
		}
		// only imaginary is non-zero: skip the real part, and decrease padding to accomoadate the trailing "j"
		if(num.imag()!=0){
			return num_to_string(num.imag(),/*pad*/pad>0?pad-1:0)+"j";
		}
		// non-complex (zero or not)
		return num_to_string(num.real(),pad);
	}
#endif


/*** getters and setters with bound guards ***/
static inline void IDX_CHECK(Eigen::Index i,Eigen::Index MAX){ if(i<0 || i>=MAX) { throw py::index_error("Index "+std::to_string(i)+" out of range 0.." + std::to_string(MAX-1)); } }
static inline void IDX2_CHECKED_TUPLE_INTS(py::tuple tuple,const Eigen::Index max2[2], Eigen::Index arr2[2]) {Eigen::Index l=py::len(tuple); if(l!=2) { PyErr_SetString(PyExc_IndexError,"Index must be integer or a 2-tuple"); throw py::error_already_set(); } for(int _i=0; _i<2; _i++) { try{ arr2[_i]=py::cast<Eigen::Index>(tuple[_i]); } catch(...){ throw py::value_error("Unable to convert "+std::to_string(_i)+"-th index to integer."); } IDX_CHECK(arr2[_i],max2[_i]); }  }

static inline string object_class_name(const py::object& obj){ return py::cast<string>(obj.attr("__class__").attr("__name__")); }

template<typename T>
bool pySeqItemCheck(PyObject* o, int i){
	try{ py::cast<T>(PySequence_GetItem(o,i)); return true; } catch(...){ return false; }
}

template<typename T>
T pySeqItemExtract(PyObject* o, int i){ return py::cast<T>(PySequence_GetItem(o,i)); }

