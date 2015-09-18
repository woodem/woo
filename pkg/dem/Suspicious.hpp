#include<woo/core/Engine.hpp>
#include<woo/pkg/dem/Particle.hpp>

struct Suspicious: public PeriodicEngine {
	WOO_DECL_LOGGER;
	bool acceptsField(Field* f) override { return dynamic_cast<DemField*>(f); }
	void run() override;
	#ifdef WOO_OPENGL
		void render(const GLViewInfo&) override;
		boost::mutex errMutex; // guard errPar and errCon while the engine is active
	#endif
	#define woo_dem_Suspicious__CLASS_BASE_DOC_ATTRS \
		Suspicious,PeriodicEngine,"Watch the simulation and signal suspicious evolutions, such as sudden increase in contact force beyond usual measure or unsual velocity.", \
			((Real,avgVel,NaN,AttrTrait<Attr::readonly>(),"Average velocity norm.")) \
			((Real,avgForce,NaN,AttrTrait<Attr::readonly>(),"Average particle force norm.")) \
			((Real,avgFn,NaN,AttrTrait<Attr::readonly>(),"Average normal force norm.")) \
			((Real,avgFt,NaN,AttrTrait<Attr::readonly>(),"Average shear force norm.")) \
			((Real,avgUn,NaN,AttrTrait<Attr::readonly>(),"Average normal overlap.")) \
			((Real,relThreshold,100,,"Break on quantity this much larger than the average.")) \
			((vector<shared_ptr<Particle>>,errPar,,,"Particle where there was some error (shown in OpenGL).")) \
			((vector<shared_ptr<Contact>>,errCon,,,"Contacts where there was some error (shown in OpenGL)"))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_Suspicious__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(Suspicious);



