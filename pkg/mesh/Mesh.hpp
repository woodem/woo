#pragma once
#include<woo/core/Field.hpp>

struct MeshData: public NodeData{
	const char* getterName() const override { return "mesh"; }
	void setDataOnNode(Node& n) override { n.setData(static_pointer_cast<MeshData>(shared_from_this())); }

	enum{VNORM_NONE=0,VNORM_SOME,VNORM_ALL};

	#define woo_mesh_MeshData__CLASS_BASE_DOC_ATTRS_PY MeshData,NodeData,"Nodal data related to mesh objects (which are comprised of many nodes and associated topology).", \
		((Vector2r,uvCoord,Vector2r(NaN,NaN),,"Parametric coordinate for texture mapping (in POV-Ray export with :obj:`~woo.dem.POVRayExport`.")) \
		((short,vNorm,VNORM_NONE,AttrTrait<Attr::namedEnum>().namedEnum({{VNORM_NONE,{"none",""}},{VNORM_SOME,{"some"}},{VNORM_ALL,{"all"}}}),"How is vertex normal defined in this node; vertex normal makes the appearance smooth (in POV-Ray, that is) by interpolating between face normal and vertex normal, shared by faces around. In nodes where smooth appearance is not desired (such as with sharp folds in mesh), set to 'none'. 'some' uses groups of similarly oriented faces around the vertex to compute vertex normal separately for those groups (not yet implemented). 'all' considers all faces around the vertex to compute average vertex normal.")) \
		,/*py*/ .def_static("_getDataOnNode",&Node::pyGetData<MeshData>).staticmethod("_getDataOnNode").def_static("_setDataOnNode",&Node::pySetData<MeshData>).staticmethod("_setDataOnNode")

	WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_mesh_MeshData__CLASS_BASE_DOC_ATTRS_PY);
};
WOO_REGISTER_OBJECT(MeshData);

template<> struct NodeData::Index<MeshData>{enum{value=Node::NODEDATA_MESH};};

