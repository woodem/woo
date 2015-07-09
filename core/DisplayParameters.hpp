#pragma once
#ifdef WOO_OPENGL

#include<woo/lib/object/Object.hpp>
#include<string>
#include<vector>
 
/* Class for storing set of display parameters.
 *
 * The interface sort of emulates map string->string (which is not handled by woo-serialization).
 *
 * The "keys" (called displayTypes) are intended to be "OpenGLRenderer" or "GLViewer" (and perhaps other).
 * The "values" are intended to be XML representation of display parameters, obtained either by woo-serialization
 * with OpenGLRenderer and saveStateToStream with QGLViewer (and GLViewer).
 *
 */

class DisplayParameters: public Object{
	public:
		//! Get value of given display type and put it in string& value and return true; if there is no such display type, return false.
		bool getValue(string displayType, std::string& value);
		//! Set value of given display type; if such display type exists, it is overwritten, otherwise a new one is created.
		void setValue(string displayType, std::string value);
	#define woo_gl_DisplayParameters__CLASS_BASE_DOC_ATTRS_PY \
		DisplayParameters,Object,"Store display parameters.", \
		((vector<string>,values,,AttrTrait<Attr::readonly>(),"")) \
		((vector<string>,displayTypes,,AttrTrait<Attr::readonly>(),"")) \
		,/*py*/;	woo::converters_cxxVector_pyList_2way<shared_ptr<DisplayParameters>>();

	WOO_DECL__CLASS_BASE_DOC_ATTRS_PY(woo_gl_DisplayParameters__CLASS_BASE_DOC_ATTRS_PY);
};
WOO_REGISTER_OBJECT(DisplayParameters);

#endif /* WOO_OPENGL */
