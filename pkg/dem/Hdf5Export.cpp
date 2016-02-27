#include<woo/pkg/dem/Hdf5Export.hpp>
#include<boost/filesystem/convenience.hpp>
#include<boost/format.hpp>

WOO_PLUGIN(dem,(DemDataTagged));
WOO_IMPL__CLASS_BASE_DOC_ATTRS(WOO_DEM_DemDataTagged__CLASS_BASE_DOC_ATTRS);

#ifdef WOO_HDF5

#include<H5Cpp.h>

WOO_PLUGIN(dem,(NodalForcesToHdf5));
WOO_IMPL__CLASS_BASE_DOC_ATTRS(WOO_DEM_NodalForceToHdf5__CLASS_BASE_DOC_ATTRS);

void NodalForcesToHdf5::run(){
	if(out.empty()) throw std::runtime_error("NodalForceToHdf5.out: empty output filename.");

	H5::H5File h5file;
	// open existing or create new file
	if(boost::filesystem::exists(out)) h5file=H5::H5File(out,H5F_ACC_RDWR);
	else h5file=H5::H5File(out,H5F_ACC_TRUNC);
	
	// open/create root group for this entire simulation
	H5::Group grp0;
	string rootGrp=scene->expandTags("/nodalforce_{id}");
	try{  H5::Exception::dontPrint(); grp0=h5file.openGroup(rootGrp); }
	catch(H5::Exception& e){ grp0=h5file.createGroup(rootGrp); }
	// create subgroup for this step
	H5::Group grp(grp0.createGroup("step_"+(boost::format("%|06|")%scene->step).str()));

	hsize_t dim[]={0,6};
	hsize_t dim11[]={1};
	// two-pass operation: first count relevant nodes, then fill lin the data
	for(const auto& n: field->nodes){ if(n->hasData<DemData>() && n->getData<DemData>().isA<DemDataTagged>()) dim[0]++; }
	// dim is now 2d array containing [number-of-nodes,6] (we store 2x3 components per node, force and torque)
	H5::DataSpace fspace(2,dim); // 2d matrix Nx6
	H5::DataSpace tspace(1,dim); // 1d matrix, Nx1
	H5::DataSpace aspace(1,dim11); // scalar (1d 1-matrix), for time value
	// enable chunking & compression
	hsize_t chunkdim[]={200,6}; // 200 is just a guess of what might have some efficiency for chunked storage
	H5::DSetCreatPropList plist;
	plist.setChunk(2,chunkdim);
	plist.setDeflate(9);
	// create datasets
	H5::DataSet fds=grp.createDataSet("forceTorque",/*type to use in the file*/H5::PredType::NATIVE_DOUBLE,fspace,plist);
	H5::DataSet tds=grp.createDataSet("tags",H5::PredType::NATIVE_INT,tspace);
	// write time value as forceTorque attribute
	H5::DataSet timeDs=grp.createDataSet("time",H5::PredType::NATIVE_DOUBLE,aspace);
	// use RowMajor, which is what HDF5 expects when passing data buffer to write (below)
	Eigen::Matrix<double,Eigen::Dynamic,6,Eigen::RowMajor> ftarr(dim[0],6); 
	Eigen::VectorXi tarr(dim[0]);
	int i=0;
	for(const auto& n: field->nodes){
		if(!n->hasData<DemData>() || !n->getData<DemData>().isA<DemDataTagged>()) continue;
		const auto& dyn(n->getData<DemData>().cast<DemDataTagged>());
		ftarr.row(i).head<3>()=dyn.force;
		ftarr.row(i).tail<3>()=dyn.torque;
		tarr[i]=dyn.tag;
		i+=1;
	}
	// write datasets, filled with Eigen data; C-style ordering (Eigen::ColumnMajor) is expected
	fds.write(ftarr.data(),/*type of data in memory*/H5::PredType::NATIVE_DOUBLE);
	tds.write(tarr.data(),H5::PredType::NATIVE_INT);
	timeDs.write(&scene->time,H5::PredType::NATIVE_DOUBLE);
	// close everything
	grp.close();
	h5file.close();
};

#endif /* WOO_HDF5 */

