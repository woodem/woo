#pragma once

#include<woo/pkg/dem/Particle.hpp>
#include<woo/core/Engine.hpp>

// not HDF5-specific (but not used elsewhere, either)
struct DemDataTagged: public DemData{
	#define WOO_DEM_DemDataTagged__CLASS_BASE_DOC_ATTRS DemDataTagged,DemData,"Add integer tag to each node, used with :obj:`NodalForcesToHdf5` so that nodes can be numbered differently than in Woo.", \
		((int,tag,-1,,"External identifier of th node (unused by Woo itself)."))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(WOO_DEM_DemDataTagged__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(DemDataTagged);

#ifdef WOO_HDF5
struct NodalForcesToHdf5: public PeriodicEngine{
	bool acceptsField(Field* f) override { return dynamic_cast<DemField*>(f); }
	void run() override;
	#define WOO_DEM_NodalForceToHdf5__CLASS_BASE_DOC_ATTRS NodalForcesToHdf5,PeriodicEngine,"Periodically export forces acting on nodes to HDF5 file. Only processes tagged nodes (having :obj:`DemDataTagged` attached, instead of plain :obj:`DemData`).", \
		((string,out,"",,"Name of the output file.")) \
		((int,deflate,9,,"Compression level for HDF5 chunked storage; valid values are 0 to 9 (will be clamped if outside)."))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(WOO_DEM_NodalForceToHdf5__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(NodalForcesToHdf5);
#endif /* WOO_HDF5 */
