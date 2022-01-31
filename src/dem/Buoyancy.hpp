#pragma once
#include<woo/core/Engine.hpp>
#include<woo/pkg/dem/Particle.hpp>
struct HalfspaceBuoyancy: public Engine{
	bool acceptsField(Field* f) override { return dynamic_cast<DemField*>(f); }
	virtual void run() override;
	#define woo_dem_HalfspaceBuoyancy_CLASS_BASE_DOC_ATTRS \
		HalfspaceBuoyancy,Engine,"Apply Buoyancy force for uninodal particles, depending on their position in the :math:`-z` half-space in local coordinate system (:obj:`node`). TODO: references to papers, wikipedia with formulas, or write formulas here.", \
		((uint,mask,uint(DemField::defaultMovableMask),,"Mask for particles Buoyancy is applied to.")) \
		((Real,liqRho,1000,,"Density of the medium.")) \
		((Real,dragCoef,0.47,,"Drag coefficient.")) \
		((bool,drag,false,,"Flag for turning water drag on or off")) \
		((Vector3r,surfPt,Vector3r(0,0,0.),,"Point on liquid surface; the surface is perpendicular to :obj:`woo.dem.DemField.gravity (which must not be zero)."))

	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_HalfspaceBuoyancy_CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(HalfspaceBuoyancy);
