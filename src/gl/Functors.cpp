#include"Functors.hpp"
#ifdef WOO_OPENGL
	WOO_PLUGIN(gl,
		(GlNodeFunctor)(GlNodeDispatcher)
		(GlFieldFunctor)(GlFieldDispatcher)
	);
#endif
