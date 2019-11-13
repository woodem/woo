#include<woo/core/EnergyTracker.hpp>
#include<woo/core/Master.hpp>
#include<boost/algorithm/string.hpp>


#ifdef WOO_VTK
	#include<vtkUniformGrid.h>
	#include<vtkPoints.h>
	#include<vtkDoubleArray.h>
	#include<vtkCellData.h>
	#include<vtkPointData.h>
	#include<vtkSmartPointer.h>
	#include<vtkZLibDataCompressor.h>
	#include<vtkXMLImageDataWriter.h>
#endif

WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_core_EnergyTracker__CLASS_BASE_DOC_ATTRS_PY);
WOO_IMPL__CLASS_BASE_DOC_ATTRS_PY(woo_core_EnergyTrackerGrid__CLASS_BASE_DOC_ATTRS_PY);


WOO_PLUGIN(core,(EnergyTracker)(EnergyTrackerGrid));
WOO_IMPL_LOGGER(EnergyTracker);
WOO_IMPL_LOGGER(EnergyTrackerGrid);


void EnergyTracker::add(const Real& val, const std::string& name, int &id, int flg, const Vector3r& xyz){
	// if ZeroDontCreate is set, the value is zero and no such energy exists, it will not be created and id will be still negative
	if(id<0) findId(name,id,flg,/*newIfNotFound*/!(val==0. && (flg&ZeroDontCreate)));
	if(id>=0){
		if(isnan(val)){ LOG_WARN("Ignoring attempt to add NaN to energy '{}'.",name); return; }
		energies.add(id,val);
		if(!isnan(xyz[0]) && grid && !(flg&IsResettable)) grid->add(val,id,xyz);
	}
}


Real EnergyTracker::total() const {
	Real ret=0; size_t sz=energies.size(); for(size_t id=0; id<sz; id++) ret+=energies.get(id); return ret;
}

Real EnergyTracker::relErr() const {
	Real sumAbs=0, sum=0; size_t sz=energies.size(); for(size_t id=0; id<sz; id++){  Real e=energies.get(id); sumAbs+=abs(e); sum+=e; } return (sumAbs>0?sum/sumAbs:0.);
}

void EnergyTracker::clear() { energies.clear(); names.clear(); flags.clear();}

py::list EnergyTracker::keys_py() const {
	py::list ret; for(const auto& p: names) ret.append(p.first); return ret;
}
py::list EnergyTracker::items_py() const {
	py::list ret; for(const auto& p: names) ret.append(py::make_tuple(p.first,energies.get(p.second))); return ret;
}

int EnergyTracker::len_py() const { return names.size(); }

void EnergyTracker::add_py(const Real& val, const std::string& name, bool reset){
	int id=-1; add(val,name,id,(reset?IsResettable:IsIncrement));
}

Real EnergyTracker::getItem_py(const std::string& name){
	int id=-1; findId(name,id,/*flags*/0,/*newIfNotFound*/false); 
	if (id<0) KeyError("Unknown energy name '"+name+"'.");
	return energies.get(id);
}

void EnergyTracker::setItem_py(const std::string& name, Real val){
	int id=-1; set(val,name,id);
}

bool EnergyTracker::contains_py(const std::string& name){
	int id=-1; findId(name,id,/*flags*/0,/*newIfNotFound*/false); return id>=0;
}


py::dict EnergyTracker::perThreadData() const {
	py::dict ret;
	std::vector<std::vector<Real> > dta=energies.getPerThreadData();
	for(pairStringInt p: names) ret[p.first.c_str()]=dta[p.second];
	return ret;
}
py::dict EnergyTracker::names_py() const{
	py::dict ret;
	for(const pairStringInt& si: names){
		ret[si.first.c_str()]=si.second;
	}
	return ret;
}


EnergyTracker::pyIterator EnergyTracker::pyIter(){ return EnergyTracker::pyIterator(static_pointer_cast<EnergyTracker>(shared_from_this())); }
EnergyTracker::pyIterator EnergyTracker::pyIterator::iter(){ return *this; }
/* python access */
EnergyTracker::pyIterator::pyIterator(const shared_ptr<EnergyTracker>& _et): et(_et), I(et->names.begin()){}
string EnergyTracker::pyIterator::next(){
	if(I==et->names.end()) woo::StopIteration();
	return (I++)->first;
}


void EnergyTracker::gridOn(const AlignedBox3r& box, Real cellSize, int maxIndex){
	grid=make_shared<EnergyTrackerGrid>();
	grid->init(box,cellSize,maxIndex);
}
void EnergyTracker::gridOff(){ grid.reset(); }

string EnergyTracker::gridToVTK(const string& out) {
	if(!grid) throw std::runtime_error("EnergyTracker: grid is not defined (use EnergyTracker.gridOn first).");
	vector<string> nn;
	for(const auto& si: names){
		assert((int)flags.size()>si.second);
		if(flags[si.second]&IsResettable) continue; // don't accumulate indices which only have total values
		if((int)nn.size()<=si.second) nn.resize(si.second+1);
		nn[si.second]=si.first;
	}
	string out2(out);
	if(!boost::algorithm::ends_with(out2,".vti")) out2+=".vti";
	grid->vtkExport(out2,nn);
	return out2;
}



#if 1
/***** GRID *******/

void EnergyTrackerGrid::add(const Real& val, int& id, const Vector3r& xyz){\
	if(data.shape()[0]==0) throw std::runtime_error("EnergyTrackerGrid::add: grid has not been initialized properly (data.shape()[0]==0).");
	if(id>=(int)data.shape()[0]) LOG_WARN("EnergyTrackerGrid: index "+to_string(id)+" beyond max energy index "+(to_string(data.shape()[0]))+" (set at initialization time).");
	if(isnan(val)) return;
	if(val==0) return;
	/*** THIS FOLLOWING WAS COPIED FROM pkg/dem/FLowAnalysis.cpp ***/
	Vector3i ijk=xyz2ijk(xyz);
	Vector3r n=(xyz-ijk2xyz(ijk))/cellSize; // normalized coordinate in the cube (0..1)x(0..1)x(0..1)
	// trilinear interpolation
	const Real& x(n[0]); const Real& y(n[1]); const Real& z(n[2]);	Real X(1-x), Y(1-y), Z(1-z);
	const int& i(ijk[0]); const int& j(ijk[1]); const int& k(ijk[2]);	int I(i+1), J(j+1), K(k+1);
	// traverse all affected points
	Real weights[]={X*Y*Z,x*Y*Z,x*y*Z,X*y*Z, X*Y*z,x*Y*z,x*y*z,X*y*z };
	Vector3i pts[]={ Vector3i(i,j,k),Vector3i(I,j,k),Vector3i(I,J,k),Vector3i(i,J,k), Vector3i(i,j,K),Vector3i(I,j,K),Vector3i(I,J,K),Vector3i(i,J,K) };
	Eigen::AlignedBox<int,3> validIjkRange(Vector3i::Zero(),Vector3i(data.shape()[1]-1,data.shape()[2]-1,data.shape()[3]-1));
	for(int ii=0; ii<8; ii++){
		if(!validIjkRange.contains(pts[ii])) continue;
		const Real& w(weights[ii]);
		data[id][pts[ii][0]][pts[ii][1]][pts[ii][2]][
			#ifdef WOO_OPENMP
				omp_get_thread_num()
			#else
				0
			#endif
		]+=w*val;
	}
}

void EnergyTrackerGrid::init(const AlignedBox3r& _box, const Real& _cellSize, int maxIndex){
	if(!(_cellSize>0)) throw std::runtime_error("EnergyTrackerGrid.cellSize: must be positive.");
	if(!(_box.volume()>0)) throw std::runtime_error("EnergyTrackerGrid.box: invalid box (volume not positive).");
	if(!(maxIndex>0)) throw std::runtime_error("EnergyTrackerGrid: maxIndex must be positive.");
	cellSize=_cellSize; box=_box;
	for(int ax:{0,1,2}){
		boxCells[ax]=std::round(box.sizes()[ax]/cellSize);
		box.max()[ax]=box.min()[ax]+boxCells[ax]*cellSize;
	}
	data.resize(boost::extents[maxIndex+1][boxCells[0]][boxCells[1]][boxCells[2]][
	#ifdef WOO_OPENMP
			omp_get_max_threads()
	#else
			1
	#endif
		]);
	std::fill(data.origin(),data.origin()+data.size(),0.);
}



#ifdef WOO_VTK
	/* COPIED FROM pkg/dem/FlowAnalysis.cpp */
	template<class vtkArrayType=vtkDoubleArray>
	vtkSmartPointer<vtkArrayType> vtkMakeArray_helper(const vtkSmartPointer<vtkUniformGrid>& grid, const string& name, size_t numComponents, bool fillZero, bool cellData, const Vector3i& boxCells){
		auto arr=vtkSmartPointer<vtkArrayType>::New();
		arr->SetNumberOfComponents(numComponents);
		arr->SetNumberOfTuples(boxCells.prod());
		arr->SetName(name.c_str());
		if(cellData) grid->GetCellData()->AddArray(arr);
		else grid->GetPointData()->AddArray(arr);
		if(fillZero){ for(int _i=0; _i<(int)numComponents; _i++) arr->FillComponent(_i,0.); }
		return arr;
	}
#endif



void EnergyTrackerGrid::vtkExport(const string& out, const vector<string>& names){
	#ifndef WOO_VTK
		throw std::runtime_error("This function is only avilable if compiled with the 'vtk' feature.");
	#else
		/* COPIED MOSTLY FROM pkg/dem/FlowAnalysis.cpp */
		/* grid setup */
		auto grid=vtkSmartPointer<vtkUniformGrid>::New();
		Vector3r cellSize3=Vector3r::Constant(cellSize);
		const bool cellData=false; // ready to be changed later, or added as argument
		if(cellData) {
			grid->SetDimensions((boxCells+Vector3i::Ones()).eval().data());
			Vector3r origin=(box.min()-.5*cellSize3);
			grid->SetOrigin(origin.data());
		} else {
			grid->SetDimensions(boxCells.data());
			grid->SetOrigin(box.min().data());
		}
		grid->SetSpacing(cellSize,cellSize,cellSize);

		for(size_t index=0; index<data.shape()[0]; index++){
			if(index>=names.size()) continue;
			if(names[index].empty()) continue;
			// string name=(names.size()>index?names[index]:"[index_"+to_string(index)+"]");
			string name=names[index];
			auto arr=vtkMakeArray_helper(grid,name,1,/*fillZero*/true,/*cellData*/cellData,boxCells);
			// traverse the grid now
			for(int i=0; i<boxCells[0]; i++){
				for(int j=0; j<boxCells[1]; j++){
					for(int k=0; k<boxCells[2]; k++){
						int ijk[]={i,j,k};
						vtkIdType dataId;
						if(cellData) dataId=grid->ComputeCellId(ijk);
						else dataId=grid->ComputePointId(ijk);
						const auto ptData(data[index][i][j][k]);
						for(size_t th=0; th<ptData.shape()[0]; th++){ 
							*(arr->GetPointer(dataId))+=ptData[th];
						}
					}
				}
			}
		}

		auto compressor=vtkSmartPointer<vtkZLibDataCompressor>::New();
		auto writer=vtkSmartPointer<vtkXMLImageDataWriter>::New();
		writer->SetFileName(out.c_str());
		#if VTK_MAJOR_VERSION==5
			writer->SetInput(grid);
		#else
			writer->SetInputData(grid);
		#endif
		// writer->SetDataModeToAscii();
	 	writer->SetCompressor(compressor);
		writer->Write();
	#endif
}
#endif
