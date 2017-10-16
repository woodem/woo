#include<woo/pkg/dem/Clustering.hpp>
#include<woo/pkg/dem/Contact.hpp>

#include<boost/graph/adjacency_list.hpp>
#include<boost/graph/connected_components.hpp>
#include<boost/graph/subgraph.hpp>
#include<boost/graph/graph_utility.hpp>
#include<boost/property_map/property_map.hpp>
#include<boost/graph/one_bit_color_map.hpp>
#include <boost/graph/stoer_wagner_min_cut.hpp>

#ifdef WOO_OPENGL
	#include<woo/lib/opengl/GLUtils.hpp>
	#include<woo/lib/base/CompUtils.hpp>
#endif


WOO_PLUGIN(dem,(ClusterMatState)(ClusterAnalysis));

WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_ClusterMatState__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_ClusterAnalysis__CLASS_BASE_DOC_ATTRS);

WOO_IMPL_LOGGER(ClusterAnalysis);


#ifdef WOO_OPENGL
	void ClusterAnalysis::render(const GLViewInfo&) {
		if(isnan(glColor)) return;
		GLUtils::AlignedBox(box,CompUtils::mapColor(glColor));
	}
#endif


void ClusterAnalysis::analyzeParticles(const vector<Particle::id_t>& ids, int level){
	if(ids.size()<2){
		LOG_TRACE("single particle, nothing to do @ conn "<<level);
		return;
	}
	LOG_WARN("== conn "<<level<<", "<<ids.size()<<" particles ==");
	// two-way mapping:
	// find particle id from vertex number i: ids[i]; complexity O(1)
	// find vertex number i from particle id: id2i[id]; complexity O(log n)
	std::map<Particle::id_t,size_t> id2i;

	typedef boost::property<boost::edge_weight_t, int> EdgeWeightProp;

	// graph with contiguous vertices (0…N-1) and edges between vertices (contacts between particles)
	typedef boost::adjacency_list<
		/*vertex storage*/boost::vecS,
		/*edge storage*/boost::vecS,
		/*graph type*/boost::undirectedS,
		/*vertex properties*/boost::no_property,
		/*edge properties*/ // boost::no_property
			EdgeWeightProp
		> Graph;
	//boost::property<boost::vertex_index_t,int>,boost::property<boost::edge_index_t,int>>> Graph;
	Graph conn(ids.size());
	LOG_TRACE("Conn "<<level<<", "<<ids.size()<<" vertices.");

	for(size_t i=0; i<ids.size(); i++){
		assert((*dem->particles)[i]);
		const auto& p((*dem->particles)[ids[i]]);
		// LOG_WARN("#"<<p->id<<": "<<p->shape->nodes[0]->pos<<" "<<box.contains(p->shape->nodes[0]->pos)<<" || "<<box.min()<<" --- "<<box.max()<<" "<<box.isEmpty());
		id2i[ids[i]]=i;
		for(const auto& idCon: p->contacts){
			if(!idCon.second->isReal()) continue;
			// This will find nothing if the other particles is not in ids at all
			// or has not been traversed yet; in the latter case, the contact will
			// be added when the loop gets to the other particle (symmetry).
			// This way, we can save one extra loop just to fill id2i first.
			const auto& iOther=id2i.find(idCon.first);
			if(iOther==id2i.end()) continue;
			boost::add_edge(i,iOther->second,EdgeWeightProp(1),conn);
			LOG_TRACE("   + "<<i<<" ⇔ "<<iOther->second);
		}
	}
	if(level>0){
		/*

		For level>0, do min-cut and remove all edges in cuts with cost==level (unit edge weight).

		1. if cheaptest min-cut has lower cost, it is an error (should have been separated at previous levels already) -- not sure?!
		2. as optimization: if the cheapest min-cut has higher cost, short-circuit and mark all as part of one cluster, return

		// http://www.boost.org/doc/libs/1_64_0/libs/graph/example/stoer_wagner.cpp
		stoer_wagner does not support finding multiple same-valued cuts, therefore:
		(a) initially define unit edge weights (that is done above, when edges are added to the graph)
		(b) run min-cut
		(c) if cost is higher than level, go to (e)
		(c) save the edges cut
		(c) increase cut edges weight to 2 (or more, that does not matter)
		(d) back to (b)
		(e) remove all saved edges (or simply edges with more than unit weight)
		(f) feed that to connected_components below
		*/
		std::list<std::pair<size_t,size_t>> cuts;
		int mincut=-1;
		do{
			auto parities=boost::make_one_bit_color_map(num_vertices(conn),boost::get(boost::vertex_index,conn));
			LOG_TRACE("  --- mincut, conn "<<level);
			mincut=boost::stoer_wagner_min_cut(conn,boost::get(boost::edge_weight,conn),boost::parity_map(parities));
			if(mincut<level){
				//throw std::runtime_error
				LOG_ERROR("ClusterAnalysis: Stoer-Wagner min-cut on contact graph reported minimum cut with weight "+to_string(mincut)+" but it should not be smaller than current connectivity level "+to_string(level));
				return;
			}
			if(mincut==level){
				// https://stackoverflow.com/questions/4810589/output-bgl-edge-weights
				/*
					how to get edge indices directly?
					asked at https://stackoverflow.com/questions/46753785/bgl-geting-edge-indices-from-stoer-wagner-min-cut
					for now, use parity map, find edges which connect non-equal partities iterating over edges
				*/
				auto eds=boost::edges(conn);
				int ie=0;
				for(auto E=eds.first; E!=eds.second; ++E){
					auto a(boost::source(*E,conn)),b(boost::target(*E,conn));
					if(boost::get(parities,a)!=boost::get(parities,b)){
						LOG_DEBUG("   | "<<a<<" ⇔ "<<b<<", weight "<<mincut<<(ie==0?" (edge weight bumped)":""));
						// only change weight for the first edge in the cut
						if (ie==0) { boost::get(boost::edge_weight,conn)[*E]=2; ie++; };
						// but remember all of them for later removal
						cuts.push_back(std::make_pair(a,b));
					}
				}
			}
		} while(mincut==level);
		LOG_TRACE("  --- no more cuts (weight "<<mincut<<" for conn "<<level<<")");
		for(const auto& ab: cuts){
			LOG_TRACE("  - "<<ab.first<<" ⇔ "<<ab.second);
			boost::remove_edge(ab.first,ab.second,conn);
		}
	}

	// connected components analysis on assembled graph
	vector<size_t> id2small(boost::num_vertices(conn));
	size_t smallNum=boost::connected_components(conn,id2small.data());
	LOG_TRACE("conn "<<level<<": "<<smallNum<<" connected components.");
	assert(id2small.size()==ids.size());
	// count particles in each cluster
	vector<int> smallSizes(smallNum,0);
	for(const auto& c: id2small) smallSizes[c]++;
	// assign contiguous labels only to clusters with at least clustMin particles
	vector<int> small2big(smallNum); // maps small cluster number (0..smallNum) to contiguous big number
	vector<int> big2small; // reverse mapping, dynamically resized
	for(size_t small=0; small<smallNum; small++){
		if(smallSizes[small]<clustMin){
			small2big[small]=-1; // invalid, does not count as cluster at all
			continue;
		}
		small2big[small]=big2small.size();
		big2small.push_back(small);
	}
	LOG_TRACE("conn "<<level<<": "<<big2small.size()<<" conn components >= clustMin (="<<clustMin<<").");

	// find existing labels
	vector<int> exBig(small2big.size(),-1);
	for(size_t i=0; i<ids.size(); i++){
		const auto& p((*dem->particles)[ids[i]]);
		const int& label=small2big[id2small[i]];
		if(label<0) continue;
		if(p->matState && p->matState->isA<ClusterMatState>()){
			const auto& ll(p->matState->cast<ClusterMatState>().labels);
			// no label for this connectivity, do nothing
			if((int)ll.size()<=level || exBig[label]<0) continue;
			// existing label mismatch, overwrite older value
			if(ll[level]!=exBig[label]){
				LOG_WARN("#"<<p->id<<": cluster level "<<level<<", label "<<exBig[label]<<" → "<<ll[level]);
				exBig[label]=ll[level];
			}
		}
	}
	// assign new labels to those clusters which have no label yet
	for(size_t i=0; i<small2big.size(); i++){
		if(exBig[i]<0) exBig[i]=++lastLabels[level];
	}
	
	// write results
	for(size_t i=0; i<ids.size(); i++){
		const auto& p((*dem->particles)[ids[i]]);
		const int& label=small2big[id2small[i]];
		if(label<0) continue;
		// foreign MatState
		if(p->matState && !p->matState->isA<ClusterMatState>()){
			switch(replMatState){
				case REPL_MATSTATE_NEVER: continue;
				case REPL_MATSTATE_ALWAYS: p->matState=make_shared<ClusterMatState>();
				case REPL_MATSTATE_ERROR: throw std::runtime_error("ClusterAnalysis: #"+to_string(p->id)+" already has p.matState "+p->matState->pyStr()+" (it could be discarded, but ClusterAnalysis.replMatState=='error').");
			}
		}
		if(!p->matState) p->matState=make_shared<ClusterMatState>();
		assert(p->matState->isA<ClusterMatState>());
		auto& st(p->matState->cast<ClusterMatState>());
		if((int)st.labels.size()<=level){
			if((int)st.labels.size()<level) LOG_WARN("ClusterAnalysis: S.dem.par["+to_string(p->id)+"].matState.labels: current size "+to_string(st.labels.size())+", adding "+to_string(level)+"-level label would skip some values, possibly leading to garbage.");
			st.labels.resize(level+1);
		}
		st.labels[level]=exBig[label];
	}

	// TODO: recurse by passing ids of each cluster separately, with level+1
	if(level<maxConn){
		for(int big=0; big<(int)big2small.size(); big++){
			int smallId=big2small[big];
			// assemble only particles in this cluster
			vector<Particle::id_t> subIds; subIds.reserve(smallSizes[smallId]);
			for(size_t i=0; i<ids.size(); i++){
				if((int)id2small[i]==smallId) subIds.push_back(ids[i]);
			}
			analyzeParticles(subIds,level+1);
		}
	}
}


void ClusterAnalysis::run(){
	dem=static_cast<DemField*>(field.get());
	if(lastLabels.size()<=maxConn) lastLabels.resize(maxConn+1,-1);

	vector<Particle::id_t> pSeq;
	// pSeq.reserve(dem->particles->size()); // will be most likely (much) smaller
	for(const auto& p: *dem->particles){
		assert(p);
		if(p->shape
			&& (mask==0 || p->mask&mask)
			&& (box.isEmpty() || box.contains(p->shape->nodes[0]->pos))
		) pSeq.push_back(p->id);
	}
	analyzeParticles(pSeq,/*level*/0);
}
