#pragma once

#ifdef WOO_OPENGL

#include"../supp/base/Types.hpp"
#include"../core/Field.hpp"

struct GlData: public NodeData{
	const char* getterName() const override { return "gl"; }
	void setDataOnNode(Node& n) override { n.setData(static_pointer_cast<GlData>(shared_from_this())); }
	bool isClipped() const { return isnan(dGlPos[0]); }
	#define woo_gl_GlData__CLASS_BASE_DOC_ATTRS_PY \
		GlData,NodeData,"Nodal data used for rendering.", \
		((Vector3r,refPos,Vector3r(NaN,NaN,NaN),AttrTrait<>().lenUnit(),"Reference position (for displacement scaling)")) \
		((Quaternionr,refOri,Quaternionr(NaN,NaN,NaN,NaN),,"Reference orientation (for rotation scaling)")) \
		((Vector3r,dGlPos,Vector3r(NaN,NaN,NaN),AttrTrait<>().lenUnit(),"Difference from real spatial position when rendered. (when [0] is NaN, the node is clipped and should not be rendered at all)")) \
		((Quaternionr,dGlOri,Quaternionr(NaN,NaN,NaN,NaN),,"Difference from real spatial orientation when rendered.")) \
		((Vector3i,dCellDist,Vector3i::Zero(),,"How much is canonicalized point from the real one.")) \
		, /* py */ \
		.def_static("_getDataOnNode",&Node::pyGetData<GlData>).def_static("_setDataOnNode",&Node::pySetData<GlData>)
	WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_gl_GlData__CLASS_BASE_DOC_ATTRS_PY);
};
WOO_REGISTER_OBJECT(GlData);
template<> struct NodeData::Index<GlData>{enum{value=Node::NODEDATA_GL};};


#endif
