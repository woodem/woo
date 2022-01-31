#include<woo/core/Field.hpp>
#include<woo/lib/base/CompUtils.hpp>
#include<woo/core/Scene.hpp>

WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR_PY(woo_core_Node__CLASS_BASE_DOC_ATTRS_CTOR_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_CTOR_PY(woo_core_Field__CLASS_BASE_DOC_ATTRS_CTOR_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_core_NodeData__CLASS_BASE_DOC_ATTRS_PY);
WOO_IMPL__CLASS_BASE_DOC(woo_core_NodeVisRep__CLASS_BASE_DOC);

WOO_PLUGIN(core,(Node)(Field)(NodeData)(NodeVisRep));

AlignedBox3r Field::renderingBbox() const {
	AlignedBox3r b;
	for(const shared_ptr<Node>& n: nodes){
		if(!n) continue;
		b.extend(n->pos);
	}
	return b;
}

const char* NodeData::getterName() const { throw std::runtime_error(pyStr()+" does not reimplement NodeData::getterName."); }
void NodeData::setDataOnNode(Node&){ throw std::runtime_error(pyStr()+" does not reimplement NodeData::setDataOnNode."); }

string Node::pyStr() const {
	std::ostringstream o;
	o<<"<"+getClassName()<<" @ "<<this<<", at ("<<pos[0]<<","<<pos[1]<<","<<pos[2]<<")>";
	return o.str();
}

Matrix3r Node::glob2loc_rank2(const Matrix3r& g){
	Matrix3r R(ori.toRotationMatrix());
	return R.transpose()*g*R;

}
Matrix3r Node::loc2glob_rank2(const Matrix3r& l){
	Matrix3r R(ori.toRotationMatrix());
	return R*l*R.transpose();
}


void Node::pyHandleCustomCtorArgs(py::args_& args, py::kwargs& kw){
	if(py::len(args)>0){
		if(py::len(args)>2) throw std::runtime_error("Node: only takes 0, 1 or 2 non-keyword arguments ("+to_string(py::len(args))+" given).");
		py::extract<Vector3r> posEx(args[0]);
		if(!posEx.check()) woo::TypeError("Node: first non-keyword argument must be Vector3 (pos)");
		pos=posEx();
		if(py::len(args)==2){
			py::extract<Quaternionr> oriEx(args[1]);
			if(!oriEx.check()) woo::TypeError("Node: second non-keyword argument must be Quaternion (ori)");
			ori=oriEx();
		}
		args=py::tuple(); // clear args
	}
	for(const char* name:{"dem","gl","mesh","sparc","clDem"}){
		if(!WOO_PY_DICT_CONTAINS(kw,name)) continue;
		auto d=py::extract<shared_ptr<NodeData>>(kw[name])();
		if(d->getterName()!=string(name)) throw std::runtime_error("Node: mismatch passing "+string(name)+"="+d->pyStr()+": shorthand for this type should be "+d->getterName()+" (not "+string(name)+").");
		d->setDataOnNode(*this);
		kw.attr("pop")(name);
	}
}


py::object Field::py_getScene(){
	if(!scene) return py::none();
	else return py::cast(static_pointer_cast<Scene>(scene->shared_from_this()));
}


