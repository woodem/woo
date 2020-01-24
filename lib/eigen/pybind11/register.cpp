#ifdef WOO_PYBIND11
#include<woo/lib/pyutil/doc_opts.hpp>
#include<woo/lib/eigen/pybind11/visitors.hpp>
// #include<pybind11/eigen.h>
namespace woo{
	void registerEigenClassesInPybind11(py::module& mod){
		// mod.attr("__name__")="woo.eigen";
		WOO_SET_DOCSTRING_OPTS;

		py::class_<Vector2r> cV2r(mod,"Vector2","3-dimensional float vector.\n\nSupported operations (``f`` if a float/int, ``v`` is a Vector3): ``-v``, ``v+v``, ``v+=v``, ``v-v``, ``v-=v``, ``v*f``, ``f*v``, ``v*=f``, ``v/f``, ``v/=f``, ``v==v``, ``v!=v``.\n\nImplicit conversion from sequence (list, tuple, ...) of 2 floats.\n\nStatic attributes: ``Zero``, ``Ones``, ``UnitX``, ``UnitY``.");
		VectorVisitor<Vector2r>::visit(cV2r);
		py::implicitly_convertible<py::tuple,Vector2r>();
		py::implicitly_convertible<py::sequence,Vector2r>();

		py::class_<Vector2i> cV2i(mod,"Vector2i","2-dimensional integer vector.\n\nSupported operations (``i`` if an int, ``v`` is a Vector2i): ``-v``, ``v+v``, ``v+=v``, ``v-v``, ``v-=v``, ``v*i``, ``i*v``, ``v*=i``, ``v==v``, ``v!=v``.\n\nImplicit conversion from sequence (list, tuple, ...) of 2 integers.\n\nStatic attributes: ``Zero``, ``Ones``, ``UnitX``, ``UnitY``.");
		VectorVisitor<Vector2i>::visit(cV2i);
		py::implicitly_convertible<py::tuple,Vector2i>();
		py::implicitly_convertible<py::sequence,Vector2i>();

		py::class_<Vector3r> cV3r(mod,"Vector3","3-dimensional float vector.\n\nSupported operations (``f`` if a float/int, ``v`` is a Vector3): ``-v``, ``v+v``, ``v+=v``, ``v-v``, ``v-=v``, ``v*f``, ``f*v``, ``v*=f``, ``v/f``, ``v/=f``, ``v==v``, ``v!=v``, plus operations with ``Matrix3`` and ``Quaternion``.\n\nImplicit conversion from sequence (list, tuple, ...) of 3 floats.\n\nStatic attributes: ``Zero``, ``Ones``, ``UnitX``, ``UnitY``, ``UnitZ``.");
		VectorVisitor<Vector3r>::visit(cV3r);
		py::implicitly_convertible<py::tuple,Vector3r>();
		py::implicitly_convertible<py::sequence,Vector3r>();

		py::class_<Vector3i> cV3i(mod,"Vector3i","3-dimensional integer vector.\n\nSupported operations (``i`` if an int, ``v`` is a Vector3i): ``-v``, ``v+v``, ``v+=v``, ``v-v``, ``v-=v``, ``v*i``, ``i*v``, ``v*=i``, ``v==v``, ``v!=v``.\n\nImplicit conversion from sequence  (list, tuple, ...) of 3 integers.\n\nStatic attributes: ``Zero``, ``Ones``, ``UnitX``, ``UnitY``, ``UnitZ``.");
		VectorVisitor<Vector3i>::visit(cV3i);
		py::implicitly_convertible<py::tuple,Vector3i>();
		py::implicitly_convertible<py::sequence,Vector3i>();

		py::class_<Matrix3r> cM3r(mod,"Matrix3","3x3 float matrix.\n\nSupported operations (``m`` is a Matrix3, ``f`` if a float/int, ``v`` is a Vector3): ``-m``, ``m+m``, ``m+=m``, ``m-m``, ``m-=m``, ``m*f``, ``f*m``, ``m*=f``, ``m/f``, ``m/=f``, ``m*m``, ``m*=m``, ``m*v``, ``v*m``, ``m==m``, ``m!=m``.\n\nStatic attributes: ``Zero``, ``Ones``, ``Identity``.");
		// create rotation matrix from quaternion
		cM3r.def(py::init<Quaternionr const &>(),py::arg("q"));
		MatrixVisitor<Matrix3r>::visit(cM3r);

		py::class_<Vector6r> cV6r(mod,"Vector6","6-dimensional float vector.\n\nSupported operations (``f`` if a float/int, ``v`` is a Vector6): ``-v``, ``v+v``, ``v+=v``, ``v-v``, ``v-=v``, ``v*f``, ``f*v``, ``v*=f``, ``v/f``, ``v/=f``, ``v==v``, ``v!=v``.\n\nImplicit conversion from sequence (list, tuple, ...) of 6 floats.\n\nStatic attributes: ``Zero``, ``Ones``.");
		VectorVisitor<Vector6r>::visit(cV6r);
		py::implicitly_convertible<py::tuple,Vector6r>();
		py::implicitly_convertible<py::sequence,Vector6r>();

		py::class_<Vector6i> cV6i(mod,"Vector6i","6-dimensional float vector.\n\nSupported operations (``f`` if a float/int, ``v`` is a Vector6): ``-v``, ``v+v``, ``v+=v``, ``v-v``, ``v-=v``, ``v*f``, ``f*v``, ``v*=f``, ``v/f``, ``v/=f``, ``v==v``, ``v!=v``.\n\nImplicit conversion from sequence (list, tuple, ...) of 6 floats.\n\nStatic attributes: ``Zero``, ``Ones``.");
		VectorVisitor<Vector6i>::visit(cV6i);
		py::implicitly_convertible<py::tuple,Vector6i>();
		py::implicitly_convertible<py::sequence,Vector6i>();

		py::class_<Matrix6r> cM6r(mod,"Matrix6","6x6 float matrix. Constructed from 4 3x3 sub-matrices, from 6xVector6 (rows).\n\nSupported operations (``m`` is a Matrix6, ``f`` if a float/int, ``v`` is a Vector6): ``-m``, ``m+m``, ``m+=m``, ``m-m``, ``m-=m``, ``m*f``, ``f*m``, ``m*=f``, ``m/f``, ``m/=f``, ``m*m``, ``m*=m``, ``m*v``, ``v*m``, ``m==m``, ``m!=m``.\n\nStatic attributes: ``Zero``, ``Ones``, ``Identity``.");
		MatrixVisitor<Matrix6r>::visit(cM6r);

		py::class_<VectorXr> cVXr(mod,"VectorX","Dynamic-sized float vector.\n\nSupported operations (``f`` if a float/int, ``v`` is a VectorX): ``-v``, ``v+v``, ``v+=v``, ``v-v``, ``v-=v``, ``v*f``, ``f*v``, ``v*=f``, ``v/f``, ``v/=f``, ``v==v``, ``v!=v``.\n\nImplicit conversion from sequence (list, tuple, ...) of X floats.");
		VectorVisitor<VectorXr>::visit(cVXr);
		py::implicitly_convertible<py::tuple,VectorXr>();
		py::implicitly_convertible<py::sequence,VectorXr>();

		py::class_<MatrixXr> cMXr(mod,"MatrixX","XxX (dynamic-sized) float matrix. Constructed from list of rows (as VectorX).\n\nSupported operations (``m`` is a MatrixX, ``f`` if a float/int, ``v`` is a VectorX): ``-m``, ``m+m``, ``m+=m``, ``m-m``, ``m-=m``, ``m*f``, ``f*m``, ``m*=f``, ``m/f``, ``m/=f``, ``m*m``, ``m*=m``, ``m*v``, ``v*m``, ``m==m``, ``m!=m``.");
		MatrixVisitor<MatrixXr>::visit(cMXr);


		py::class_<Quaternionr> cQ(mod,"Quaternion","Quaternion representing rotation.\n\nSupported operations (``q`` is a Quaternion, ``v`` is a Vector3): ``q*q`` (rotation composition), ``q*=q``, ``q*v`` (rotating ``v`` by ``q``), ``q==q``, ``q!=q``.\n\nStatic attributes: ``Identity``.\n\n.. note:: Quaternion is represented as axis-angle when printed (e.g. ``Identity`` is ``Quaternion((1,0,0),0)``, and can also be constructed from the axis-angle representation. This is however different from the data stored inside, which can be accessed by indices ``[0]`` (:math:`x`), ``[1]`` (:math:`y`), ``[2]`` (:math:`z`), ``[3]`` (:math:`w`). To obtain axis-angle programatically, use :obj:`Quaternion.toAxisAngle` which returns the tuple.");
		QuaternionVisitor<Quaternionr>::visit(cQ);

		py::class_<AlignedBox3r> cAB3r(mod,"AlignedBox3","Axis-aligned box object, defined by its minimum and maximum corners");
		AabbVisitor<AlignedBox3r>::visit(cAB3r);

		 py::class_<AlignedBox2r> cAB2r(mod,"AlignedBox2","Axis-aligned box object in 2d, defined by its minimum and maximum corners");
		 AabbVisitor<AlignedBox2r>::visit(cAB2r);


	};
};
#endif
