#pragma once
#ifdef WOO_OPENGL

#include<woo/pkg/dem/Particle.hpp>
#include<woo/pkg/dem/Gl1_DemField.hpp>

class Gl1_CPhys: public GlCPhysFunctor{	
	public:
		void go(const shared_ptr<CPhys>&,const shared_ptr<Contact>&, const GLViewInfo&) override;
	#define woo_dem_Gl1_CPhys__CLASS_BASE_DOC_ATTRS \
		Gl1_CPhys,GlCPhysFunctor,"Renders :obj:`CPhys` objects as cylinders of which diameter and color depends on :obj:`CPhys:force` normal (:math:`x`) component.", \
		((shared_ptr<ScalarRange>,range,make_shared<ScalarRange>(),,"Range for normal force")) \
		((shared_ptr<ScalarRange>,shearRange,make_shared<ScalarRange>(),,"Range for absolute value of shear force")) \
		((bool,shearColor,false,,"Set color by shear force rather than by normal force. (Radius still depends on normal force)")) \
		((int,signFilter,0,,"If non-zero, only display contacts with negative (-1) or positive (+1) normal forces; if zero, all contacts will be displayed.")) \
		((Real,relMaxRad,.01,,"Relative radius for maximum forces")) \
		((int,slices,6,,"Number of cylinder slices")) \
		((Vector2i,slices_range,Vector2i(4,16),AttrTrait<>().noGui(),"Range for slices"))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_Gl1_CPhys__CLASS_BASE_DOC_ATTRS);
	RENDERS(CPhys);
};
WOO_REGISTER_OBJECT(Gl1_CPhys);
#endif
