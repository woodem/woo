

#ifdef WOO_OPENGL
#include"../supp/opengl/GLUtils.hpp"
#include"Gl1_Node.hpp"
#include"Renderer.hpp"

WOO_PLUGIN(gl,(Gl1_Node));
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_Gl1_Node__CLASS_BASE_DOC_ATTRS);

void Gl1_Node::go(const shared_ptr<Node>& node, const GLViewInfo& viewInfo){
	if(wd<=0) return;
	glLineWidth(wd);
	Renderer::glScopedName name(viewInfo,node);

	if(len<=0 || viewInfo.renderer->fastDraw){
		glPointSize(wd);
		glBegin(GL_POINTS); glVertex3f(0,0,0); glEnd();
	} else {
		for(int i=0; i<3; i++){
			Vector3r pt=Vector3r::Zero(); pt[i]=len*viewInfo.sceneRadius; const Vector3r& color=viewInfo.renderer->axisColor(i);
			GLUtils::GLDrawLine(Vector3r::Zero(),pt,color);
			// if(axesLabels) GLUtils::GLDrawText(string(i==0?"x":(i==1?"y":"z")),pt,color);
		}
	}
	glLineWidth(1);
};

#endif

