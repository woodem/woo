#pragma once
#include"Collision.hpp"
#include"Contact.hpp"

//#define WOO_ABBY

#ifdef WOO_ABBY
	#include"../lib/aabbcc/abby.hpp"
#else
	#include"../lib/aabbcc/AABB.h"
#endif

namespace aabb{ struct Tree; };

struct AabbTreeCollider: public AabbCollider{
	#ifdef WOO_ABBY
		typedef abby::tree<Particle::id_t,Vector3r> Tree3d;
	#else
		typedef ::aabb::Tree Tree3d;
		std::vector<Vector3i> boxPeriods;
	#endif
	shared_ptr<Tree3d> tree;

	WOO_DECL_LOGGER;
	/* inherited interface */
	bool acceptsField(Field* f) override { return dynamic_cast<DemField*>(f); }
	DemField* dem;
	// void postLoad(AabbTreeCollider&, void*);
	// void selfTest() override;
	#if 0
		#ifdef WOO_OPENGL
			void render(const GLViewInfo&) override;
		#endif
	#endif
	void run() override;
	// forces reinitialization
	void invalidatePersistentData() override;

	AlignedBox3r canonicalBox(const Particle::id_t&, const Aabb& aabb);

	Vector3i pyGetPeriod(const Particle::id_t& id);
	AlignedBox3r pyGetAabb(const Particle::id_t& id);

	public:
	//! Predicate called from loop within ContactContainer::erasePending
	bool shouldBeRemoved(const shared_ptr<Contact> &C, Scene* scene) const {
		if(C->pA.expired() || C->pB.expired()) return true; //remove contact where constituent particles have been deleted
		Particle::id_t id1=C->leakPA()->id, id2=C->leakPB()->id;
		if(!tree) return false;
		#ifdef WOO_ABBY
			return !tree->get_aabb(id1).overlaps(tree->get_aabb(id2),/*touchIsOverlap*/true);
		#else
			return !tree->getAABB(id1).overlaps(tree->getAABB(id2),/*touchIsOverlap*/true);
		#endif
		//if(!scene->periodic) return !spatialOverlap(id1,id2);
		//else { Vector3i periods; return !spatialOverlapPeri(id1,id2,scene,periods); }
	}


	#define woo_dem_AabbTreeCollider__CLASS_BASE_DOC_ATTRS_PY \
		AabbTreeCollider,AabbCollider,ClassTrait().doc("Collider based on AABB hierarchy."), \
		/*attrs*/ \
		,/*py*/ .def("getAabb",&AabbTreeCollider::pyGetAabb,py::arg("id")).def("getPeriod",&AabbTreeCollider::pyGetPeriod,py::arg("id"))


	WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_dem_AabbTreeCollider__CLASS_BASE_DOC_ATTRS_PY);
};
WOO_REGISTER_OBJECT(AabbTreeCollider);
