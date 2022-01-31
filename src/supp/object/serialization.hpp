#pragma once

#include<woo/lib/base/Math.hpp>
#include<boost/multi_array.hpp>

#ifdef WOO_CEREAL
	// Archive::is_loading not in cereal (until 1.2.2), create compatibility template archive_is_loading.
	template<class A> constexpr bool archive_is_loading(){ return false; }
	#include<cereal/archives/binary.hpp>
	template<> constexpr bool archive_is_loading<cereal::BinaryInputArchive>(){ return true; }
	#ifndef WOO_NOXML
		#include<cereal/archives/xml.hpp>
		template<> constexpr bool archive_is_loading<cereal::XMLInputArchive>(){ return true; }
	#endif

	#ifndef WOO_NOXML
		#error Cereal currently does not work with XML serialization (binary_data equivalent not available for XML); you have to compile with the "noxml" feature.
	#endif

	#include<cereal/cereal.hpp>
	#include<cereal/types/array.hpp>
	#include<cereal/types/vector.hpp>
	#include<cereal/types/list.hpp>
	#include<cereal/types/map.hpp>
	#include<cereal/types/memory.hpp>
	#include<cereal/types/polymorphic.hpp>
	#include<cereal/types/set.hpp>
	#include<cereal/types/string.hpp>
	#include<cereal/types/tuple.hpp>
	#include<cereal/types/tuple.hpp>
	// this is the closest equivalent in cereal for make_array, but only works for binary archives...
	// XXX: copied definition of binary_data verbatim
 	namespace cereal { template <class T> inline BinaryData<T> make_array(T && data, size_t size ){ return {std::forward<T>(data), size}; } }
	#define BOOST_SERIALIZATION_NVP CEREAL_NVP
#else
	// silence boost internal warnings about deprecated headers
	#define BOOST_DISABLE_PRAGMA_MESSAGE
	template<class A> constexpr bool archive_is_loading(){ return A::is_loading::value; }
	#include<boost/archive/binary_oarchive.hpp>
	#include<boost/archive/binary_iarchive.hpp>
	#ifndef WOO_NOXML
		#include<boost/archive/xml_oarchive.hpp>
		#include<boost/archive/xml_iarchive.hpp>
	#endif

	#include<boost/serialization/export.hpp> // must come after all supported archive types
	#include<boost/serialization/base_object.hpp>
	#include<boost/serialization/shared_ptr.hpp>
	#include<boost/serialization/weak_ptr.hpp>
	#include<boost/serialization/list.hpp>
	#include<boost/serialization/vector.hpp>
	#include<boost/serialization/map.hpp>
	#include<boost/serialization/set.hpp>
	#include<boost/serialization/nvp.hpp>
	#include<boost/serialization/is_bitwise_serializable.hpp>
	#include<boost/serialization/array.hpp>
	// backwards-compatibility:
	// this exposes access, make_nvp and other as cereal::*
	namespace cereal=boost::serialization;
	namespace boost { namespace serialization {
		typedef boost::archive::binary_iarchive BinaryInputArchive;
		typedef boost::archive::binary_oarchive BinaryOutputArchive;
		#ifndef WOO_NOXML
			typedef boost::archive::xml_iarchive XMLInputArchive;
			typedef boost::archive::xml_oarchive XMLOutputArchive;
		#endif
	}}
#endif


// declare supported archive types so that we can declare the templates explcitily in headers
// and specialize them explicitly in implementation files

// this already uses aliases which point to boost::serialization archives, if cereal is not used
#ifndef WOO_NOXML
	#define WOO_BOOST_ARCHIVES (cereal::BinaryInputArchive)(cereal::BinaryOutputArchive)(cereal::XMLInputArchive)(cereal::XMLOutputArchive)
#else
	#define WOO_BOOST_ARCHIVES (cereal::BinaryInputArchive)(cereal::BinaryOutputArchive)
#endif


/*
 * Serialization of math classes
 */
#ifdef WOO_CEREAL
	namespace cereal {
		template <class Archive, class Derived> inline
		typename std::enable_if<traits::is_output_serializable<BinaryData<typename Derived::Scalar>, Archive>::value, void>::type
		save(Archive & ar, Eigen::PlainObjectBase<Derived> const & m)
		{
			typedef Eigen::PlainObjectBase<Derived> ArrT;
			if(ArrT::RowsAtCompileTime==Eigen::Dynamic) ar(m.rows());
			if(ArrT::ColsAtCompileTime==Eigen::Dynamic) ar(m.cols());
			ar(binary_data(m.data(),m.size()*sizeof(typename Derived::Scalar)));
		}

		template <class Archive, class Derived> inline
		typename std::enable_if<traits::is_input_serializable<BinaryData<typename Derived::Scalar>, Archive>::value, void>::type
		load(Archive & ar, Eigen::PlainObjectBase<Derived> & m)
		{
			typedef Eigen::PlainObjectBase<Derived> ArrT;
			Eigen::Index rows=ArrT::RowsAtCompileTime, cols=ArrT::ColsAtCompileTime;
			if(rows==Eigen::Dynamic) ar(rows);
			if(cols==Eigen::Dynamic) ar(cols);
			m.resize(rows,cols);
			ar(binary_data(m.data(),static_cast<std::size_t>(rows*cols*sizeof(typename Derived::Scalar))));
		}

		template<class Archive, typename Scalar, size_t Dimension>
		void serialize(Archive & ar, boost::multi_array<Scalar,Dimension> & a, const unsigned int version){
			typedef typename boost::multi_array<Scalar,Dimension>::size_type size_type;
			boost::array<size_type,Dimension> shape;
			if(!archive_is_loading<Archive>()) std::memcpy(shape.data(),a.shape(),sizeof(size_type)*shape.size());
			ar & cereal::make_nvp("shape",cereal::binary_data(shape.data(),shape.size()));
			if(archive_is_loading<Archive>()) a.resize(shape);
			ar & cereal::make_nvp("data",cereal::binary_data(a.data(),a.num_elements()));
		};

		template<class Archive>
		void serialize(Archive & ar, Quaternionr & g, const unsigned int version)
		{
			Real &w=g.w(), &x=g.x(), &y=g.y(), &z=g.z();
			ar & CEREAL_NVP(w) & CEREAL_NVP(x) & CEREAL_NVP(y) & CEREAL_NVP(z);
		}

		template<class Archive, typename Scalar, int AmbientDim>
		void serialize(Archive & ar, Eigen::AlignedBox<Scalar,AmbientDim>& b, const unsigned int version){
			auto& min(b.min()); auto& max(b.max());
			ar & CEREAL_NVP(min) & CEREAL_NVP(max);
		}


	} // namespace cereal
#else
	// fast serialization (no version infor and no tracking) for basic math types
	// http://www.boost.org/doc/libs/1_42_0/libs/serialization/doc/traits.html#bitwise
	BOOST_IS_BITWISE_SERIALIZABLE(Vector2r);
	BOOST_IS_BITWISE_SERIALIZABLE(Vector2i);
	BOOST_IS_BITWISE_SERIALIZABLE(Vector3r);
	BOOST_IS_BITWISE_SERIALIZABLE(Vector3i);
	BOOST_IS_BITWISE_SERIALIZABLE(Vector4r);
	BOOST_IS_BITWISE_SERIALIZABLE(Vector6r);
	BOOST_IS_BITWISE_SERIALIZABLE(Vector6i);
	BOOST_IS_BITWISE_SERIALIZABLE(Quaternionr);
	BOOST_IS_BITWISE_SERIALIZABLE(Matrix3r);
	BOOST_IS_BITWISE_SERIALIZABLE(Matrix3i);
	BOOST_IS_BITWISE_SERIALIZABLE(Matrix6r);
	BOOST_IS_BITWISE_SERIALIZABLE(MatrixXr);
	BOOST_IS_BITWISE_SERIALIZABLE(VectorXr);
	BOOST_IS_BITWISE_SERIALIZABLE(AlignedBox2r);
	BOOST_IS_BITWISE_SERIALIZABLE(AlignedBox2i);
	BOOST_IS_BITWISE_SERIALIZABLE(AlignedBox3r);
	BOOST_IS_BITWISE_SERIALIZABLE(AlignedBox3i);

	namespace boost {
	namespace serialization {

	template<class Archive>
	void serialize(Archive & ar, Vector2r & g, const unsigned int version){
		ar & cereal::make_nvp("data",boost::serialization::make_array(g.data(),2));
	}

	template<class Archive>
	void serialize(Archive & ar, Vector2i & g, const unsigned int version){
		ar & cereal::make_nvp("data",boost::serialization::make_array(g.data(),2));
	}

	template<class Archive>
	void serialize(Archive & ar, Vector3r & g, const unsigned int version)
	{
		ar & cereal::make_nvp("data",boost::serialization::make_array(g.data(),3));
	}

	template<class Archive>
	void serialize(Archive & ar, Vector3i & g, const unsigned int version){
		ar & cereal::make_nvp("data",boost::serialization::make_array(g.data(),3));
	}

	template<class Archive>
	void serialize(Archive & ar, Vector4r & g, const unsigned int version)
	{
		ar & cereal::make_nvp("data",boost::serialization::make_array(g.data(),4));
	}

	template<class Archive>
	void serialize(Archive & ar, Vector6r & g, const unsigned int version){
		ar & cereal::make_nvp("data",boost::serialization::make_array(g.data(),6));
	}

	template<class Archive>
	void serialize(Archive & ar, Vector6i & g, const unsigned int version){
		ar & cereal::make_nvp("data",boost::serialization::make_array(g.data(),6));
	}

	template<class Archive>
	void serialize(Archive & ar, Quaternionr & g, const unsigned int version)
	{
		Real &w=g.w(), &x=g.x(), &y=g.y(), &z=g.z();
		ar & BOOST_SERIALIZATION_NVP(w) & BOOST_SERIALIZATION_NVP(x) & BOOST_SERIALIZATION_NVP(y) & BOOST_SERIALIZATION_NVP(z);
	}

	template<class Archive>
	void serialize(Archive & ar, AlignedBox2r & b, const unsigned int version){
		Vector2r& min(b.min()); Vector2r& max(b.max());
		ar & BOOST_SERIALIZATION_NVP(min) & BOOST_SERIALIZATION_NVP(max);
	}

	template<class Archive>
	void serialize(Archive & ar, AlignedBox3r & b, const unsigned int version){
		Vector3r& min(b.min()); Vector3r& max(b.max());
		ar & BOOST_SERIALIZATION_NVP(min) & BOOST_SERIALIZATION_NVP(max);
	}

	template<class Archive>
	void serialize(Archive & ar, Matrix3r & m, const unsigned int version){
		ar & cereal::make_nvp("data",boost::serialization::make_array(m.data(),3*3));
	}

	template<class Archive>
	void serialize(Archive & ar, Matrix3i & m, const unsigned int version){
		ar & cereal::make_nvp("data",boost::serialization::make_array(m.data(),3*3));
	}

	template<class Archive>
	void serialize(Archive & ar, Matrix6r & m, const unsigned int version){
		ar & cereal::make_nvp("data",boost::serialization::make_array(m.data(),6*6));
	}

	template<class Archive>
	void serialize(Archive & ar, VectorXr & v, const unsigned int version){
		int size=v.size();
		ar & BOOST_SERIALIZATION_NVP(size);
		if(Archive::is_loading::value) v.resize(size);
		ar & cereal::make_nvp("data",boost::serialization::make_array(v.data(),size));
	};

	template<class Archive>
	void serialize(Archive & ar, MatrixXr & m, const unsigned int version){
		int rows=m.rows(), cols=m.cols();
		ar & BOOST_SERIALIZATION_NVP(rows) & BOOST_SERIALIZATION_NVP(cols);
		if(Archive::is_loading::value) m.resize(rows,cols);
		ar & cereal::make_nvp("data",boost::serialization::make_array(m.data(),cols*rows));
	};

	template<class Archive, typename Scalar, size_t Dimension>
	void serialize(Archive & ar, boost::multi_array<Scalar,Dimension> & a, const unsigned int version){
		typedef typename boost::multi_array<Scalar,Dimension>::size_type size_type;
		boost::array<size_type,Dimension> shape;
		if(!Archive::is_loading::value) std::memcpy(shape.data(),a.shape(),sizeof(size_type)*shape.size());
		ar & cereal::make_nvp("shape",boost::serialization::make_array(shape.data(),shape.size()));
		if(Archive::is_loading::value) a.resize(shape);
		ar & cereal::make_nvp("data",boost::serialization::make_array(a.data(),a.num_elements()));
	};


	} // namespace serialization
	} // namespace boost

#endif /* WOO_CEREAL */



