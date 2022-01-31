#pragma once

#include<woo/lib/base/Types.hpp>
#include<woo/lib/base/Math.hpp>
#include<woo/lib/object/serialization.hpp>

namespace woo{
	class Pickler{
		static bool initialized;
		static py::handle cPickle_dumps;
		static py::handle cPickle_loads;
		static void ensureInitialized();
		public:
			static std::string dumps(py::object o);
			static py::object loads(const std::string&);
	};
}


#ifdef WOO_CEREAL
namespace cereal{
#else
	namespace boost{namespace serialization{
#endif

	template<class Archive>
	void serialize(Archive & ar, py::object& obj, const unsigned int version){
		std::string pickled;
		if(!archive_is_loading<Archive>()) pickled=woo::Pickler::dumps(obj);
		ar & cereal::make_nvp("pickled",pickled);
		if(archive_is_loading<Archive>()) obj=woo::Pickler::loads(pickled);
	};
	#define SERIALIZE_PY_TYPE(PY_TYPE) \
		template<class Archive> void serialize(Archive & ar, PY_TYPE& obj, const unsigned int version){ \
			std::string pickled; \
			if(!archive_is_loading<Archive>()) pickled=woo::Pickler::dumps(py::object(obj)); \
			ar & cereal::make_nvp("pickled",pickled); \
			if(archive_is_loading<Archive>()) obj=py::cast<PY_TYPE>(woo::Pickler::loads(pickled)); \
		};
	SERIALIZE_PY_TYPE(py::dict);
	SERIALIZE_PY_TYPE(py::tuple);
	SERIALIZE_PY_TYPE(py::list);
	// add other types here
#ifdef WOO_CEREAL
}
#else
}}
#endif
