#pragma once

#include<woo/pkg/dem/Particle.hpp>


struct ClusterMatState: public MatState {
	size_t getNumScalars() const override { return labels.size()*2; }
	string getScalarName(int index) override {
		if(index<0 || index>=2*(int)labels.size()) return "";
		if(index%2==0) return ("conn-"+to_string(index/2)).c_str();
		return ("conn-"+to_string(index/2)+"%8").c_str();
	}
	Real getScalar(int index, const long& step, const Real& smooth=0) override {
		if(index<0 || index>=2*(int)labels.size()) return NaN;
		if(index%2==0) return labels[index/2];
		return labels[index/2]%8;
	}
	#define woo_dem_ClusterMatState__CLASS_BASE_DOC_ATTRS \
		ClusterMatState,MatState,"Hold cluster labels for each degree of connectivity.", \
		((vector<int>,labels,,,"Cluster label for 0,1,2,... connectivity."))

	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_ClusterMatState__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(ClusterMatState);

struct ClusterAnalysis: public PeriodicEngine {
	WOO_DECL_LOGGER;
	bool acceptsField(Field* f) override { return dynamic_cast<DemField*>(f); }
	DemField* dem; // temp only
	void analyzeParticles(const vector<Particle::id_t>& ids, int level);
	void run() override;

	#ifdef WOO_OPENGL
		void render(const GLViewInfo&) override;
	#endif


	enum{REPL_MATSTATE_ALWAYS,REPL_MATSTATE_NEVER,REPL_MATSTATE_ERROR};

	#define woo_dem_ClusterAnalysis__CLASS_BASE_DOC_ATTRS \
		ClusterAnalysis,PeriodicEngine,"Analyze particle connectivity and assign cluster numbers to them.", \
		((AlignedBox3r,box,,,"Limit particles considered in the analysis to this box only. If not specified, analyze all particles in the simulation.")) \
		((int,mask,0,,"Particles to consider in cluster analysis. If 0, consider all particles.")) \
		((int,clustMin,5,,"Minimum number of particles to mark them as clustered.")) \
		((vector<int>,lastLabels,,AttrTrait<Attr::readonly>(),"Next free labels to be used for respective connectivity level.")) \
		((int,maxConn,0,,"Maximum connectivity (recursion) for cluster analysis. Only 0 is implemented so far.")) \
		((int,replMatState,REPL_MATSTATE_ALWAYS,AttrTrait<Attr::namedEnum>().namedEnum({{REPL_MATSTATE_ALWAYS,{"always","yes","ok"}},{REPL_MATSTATE_NEVER,{"never","no"}},{REPL_MATSTATE_ERROR,{"error"}}}),"What to do when an existing :obj:`MatState` is attached to a particle but it is not a :obj:`ClusterMatState`.")) \
		((Real,glColor,.4,,"Color for rendering the box (NaN to disable rendering)")) 

	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_ClusterAnalysis__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(ClusterAnalysis);


