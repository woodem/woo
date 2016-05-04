#pragma once

#include<woo/pkg/dem/Particle.hpp>
#include<woo/core/Engine.hpp>

// not HDF5-specific (but not used elsewhere, either)
struct DemDataTagged: public DemData{
	#define WOO_DEM_DemDataTagged__CLASS_BASE_DOC_ATTRS DemDataTagged,DemData,"Add integer tag to each node, used with :obj:`ForcesToHdf5` so that nodes can be numbered differently than in Woo.", \
		((int,tag,-1,,"External identifier of th node (unused by Woo itself)."))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(WOO_DEM_DemDataTagged__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(DemDataTagged);

#ifdef WOO_HDF5
struct ForcesToHdf5: public PeriodicEngine{
	bool acceptsField(Field* f) override { return dynamic_cast<DemField*>(f); }
	void run() override;
	enum{ HDF_NODAL, HDF_CONTACT};
	#define WOO_DEM_ForceToHdf5__CLASS_BASE_DOC_ATTRS ForcesToHdf5,PeriodicEngine,"Periodically export nodal/contact forces to HDF5 file. Nodal forces are only exported for tagged nodes (having :obj:`DemDataTagged` attached, instead of plain :obj:`DemData`) and include both force and torque. Contact forces only export force (not torque, which is zero for most models, but the adaptation would be easy).", \
		((int,what,HDF_NODAL,AttrTrait<Attr::namedEnum>().namedEnum({{HDF_NODAL,{"node","nodes","nodal"}},{HDF_CONTACT,{"contact","contacts"}}}),"Select whether to export nodal forces (default) or contact forces.")) \
		((int,contMask,DemField::defaultLoneMask,,"Only export contacts where at least one particle matches this mask. If zero, match all contacts. If this :obj:`Contact.pA` has matching mask, inverts force sense; this has as result that (unless both particles match) the force is exported as it acts on the matching particle.")) \
		((string,out,"",,"Name of the output file.")) \
		((int,deflate,9,,"Compression level for HDF5 chunked storage; valid values are 0 to 9 (will be clamped if outside)."))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(WOO_DEM_ForceToHdf5__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(ForcesToHdf5);

struct NodalForcesToHdf5: public ForcesToHdf5{
	#define WOO_DEM_NodalForceToHdf5__CLASS_BASE_DOC NodalForcesToHdf5,ForcesToHdf5,"Legacy class which only serves for backwards compatibility."
	WOO_DECL__CLASS_BASE_DOC(WOO_DEM_NodalForceToHdf5__CLASS_BASE_DOC);
};
WOO_REGISTER_OBJECT(NodalForcesToHdf5);
#endif /* WOO_HDF5 */
