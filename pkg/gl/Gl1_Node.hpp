#pragma once

#ifdef WOO_OPENGL

#include<woo/pkg/gl/Functors.hpp>
struct Gl1_Node: public GlNodeFunctor{
	virtual void go(const shared_ptr<Node>&, const GLViewInfo&) override;
	RENDERS(Node);
	#define woo_dem_Gl1_Node__CLASS_BASE_DOC_ATTRS \
		Gl1_Node,GlNodeFunctor,"Render generic :obj:`woo.dem.Node`.", \
		((int,wd,1,,"Local axes line width in pixels")) \
		((Vector2i,wd_range,Vector2i(0,5),AttrTrait<>().noGui(),"Range for width")) \
		((Real,len,.05,,"Relative local axes line length in pixels, relative to scene radius; if non-positive, only points are drawn")) \
		((Vector2r,len_range,Vector2r(0.,.1),AttrTrait<>().noGui(),"Range for len"))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_Gl1_Node__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(Gl1_Node);
#endif


