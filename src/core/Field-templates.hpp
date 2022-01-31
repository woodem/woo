#pragma once

#include<woo/core/Scene.hpp>
#include<woo/core/Field.hpp>


template<typename FieldT> bool Field_sceneHasField(const shared_ptr<Scene>& scene)
{
	for(const shared_ptr<Field>& f: scene->fields)
	if(dynamic_pointer_cast<FieldT>(f)) return true;
	return false;
}
template<typename FieldT> shared_ptr<FieldT>
Field_sceneGetField(const shared_ptr<Scene>& scene){
	for(const shared_ptr<Field>& f: scene->fields) if(dynamic_pointer_cast<FieldT>(f)) return static_pointer_cast<FieldT>(f);
	WOO_LOCAL_LOGGER(Field);
	auto f=make_shared<FieldT>();
	LOG_INFO("Creating new field #{} {}",scene->fields.size(),f->pyStr());
	scene->fields.push_back(f);
	return f;
}


