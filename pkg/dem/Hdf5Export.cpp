#include<boost/filesystem/convenience.hpp>
#include<woo/pkg/dem/Hdf5Export.hpp>
#include<boost/format.hpp>

#include<woo/pkg/dem/Contact.hpp>

WOO_PLUGIN(dem,(DemDataTagged));
WOO_IMPL__CLASS_BASE_DOC_ATTRS(WOO_DEM_DemDataTagged__CLASS_BASE_DOC_ATTRS);

#ifdef WOO_HDF5

#include<H5Cpp.h>

WOO_PLUGIN(dem,(ForcesToHdf5)(NodalForcesToHdf5));
WOO_IMPL__CLASS_BASE_DOC_ATTRS(WOO_DEM_ForceToHdf5__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC(WOO_DEM_NodalForceToHdf5__CLASS_BASE_DOC);

void ForcesToHdf5::run(){
	if(out.empty()) throw std::runtime_error("ForceToHdf5.out: empty output filename.");
	if(!(what==HDF_NODAL || what==HDF_CONTACT)) throw std::runtime_error("ForceToHdf5.what: invalid value "+to_string(what)+".");

	// we don't need automatic printing of H5 errors on stderr
	// enough to get information from the exception
	H5::Exception::dontPrint();

	/* H5::Exception does not derive from std::exception. Therefore wrap all HDF5 calls
	with try-catch and raise std::runtime_error instead so that Scene bg thread can handle
	it (otherwise the exception would be unhandled, leading to crash. */
	try{
		H5::H5File h5file;
		// open existing or create new file
		if(boost::filesystem::exists(out)) h5file=H5::H5File(out,H5F_ACC_RDWR);
		else h5file=H5::H5File(out,H5F_ACC_TRUNC);
		
		// open/create root group for this entire simulation
		H5::Group grp0,grp;
		string rootGrp=scene->expandTags(what==HDF_NODAL?"/nodalforce_{id}":"/contactforces_{id}");
		try{ grp0=h5file.openGroup(rootGrp); }
		catch(H5::Exception& e){ grp0=h5file.createGroup(rootGrp); }
		// create subgroup for this step
		string grpName("step_"+(boost::format("%|06|")%scene->step).str());
		grp=grp0.createGroup(grpName);

		const hsize_t nCols=6; // force+torque for nodes, force+coordinates for contacts
		hsize_t dim[]={0,nCols};
		hsize_t dim11[]={1};

		// contact matching expression (used in the first and second pass, so keep it in one place only)
		auto contactMatch=[this](const shared_ptr<Contact>& C){ return C->isReal() && (contMask==0 || (C->leakPA()->mask&contMask) || (C->leakPB()->mask&contMask)); };

		// two-pass operation: first count relevant nodes or contacts, then fill in the data
		if(what==HDF_NODAL){ for(const auto& n: field->nodes){ if(n->hasData<DemData>() && n->getData<DemData>().isA<DemDataTagged>()) dim[0]++; } }
		else{ for(const auto& C: *(field->cast<DemField>().contacts)){ if(contactMatch(C)) dim[0]++; } }

		// dim is now 2d array containing [number-of-records,nCols] (we store nCols components per record, i.e. per node (force, torque) or per contact (force, coordinate)
		H5::DataSpace fspace(2,dim); // 2d matrix N x nCols
		H5::DataSpace tspace(1,dim); // 1d matrix, Nx1
		// enable chunking & compression
		// 400 is just a guess of what might have some efficiency for chunked storage
		// chunk size may not be smaller than dataset size, otherwise H5 gives an erro when creating dataset
		hsize_t chunkdim[]={min((hsize_t)400,dim[0]),nCols}; 
		H5::DSetCreatPropList plist;
		plist.setChunk(2,chunkdim);
		plist.setDeflate(min(max(deflate,0),9));
		if(what==HDF_NODAL){
			// create datasets
			H5::DataSet fds=grp.createDataSet("forceTorque",/*type to use in the file*/H5::PredType::NATIVE_DOUBLE,fspace,plist);
			H5::DataSet tds=grp.createDataSet("tags",H5::PredType::NATIVE_INT,tspace);
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
		} else {
			H5::DataSet cfds=grp.createDataSet("coordForce",/*type to use in the file*/H5::PredType::NATIVE_DOUBLE,fspace,plist);
			Eigen::Matrix<double,Eigen::Dynamic,6,Eigen::RowMajor> cfarr(dim[0],6);
			int i=0;
			for(const auto& C: *(field->cast<DemField>().contacts)){
				if(!contactMatch(C)) continue;
				Vector3r fg=C->geom->node->loc2glob(C->phys->force); // convert to global
				// adjust sense so that it acts in the right direction
				if(C->leakPA()->mask&contMask) fg*=-1;
				cfarr.row(i).head<3>()=C->geom->node->pos;
				cfarr.row(i).tail<3>()=fg;
				i+=1;
			}
			cfds.write(cfarr.data(),H5::PredType::NATIVE_DOUBLE);
		}
		// write time value as a separate 1x1 dataset
		H5::DataSpace aspace(1,dim11); // scalar (1d 1-matrix), for time value
		H5::DataSet timeDs=grp.createDataSet("time",H5::PredType::NATIVE_DOUBLE,aspace);
		timeDs.write(&scene->time,H5::PredType::NATIVE_DOUBLE);
		// close everything
		grp.close();
		h5file.close();
	} catch(H5::Exception& e){
		std::ostringstream oss;
		e.walkErrorStack(H5E_WALK_DOWNWARD,[](unsigned int n, const H5E_error_t* err, void* oss_)->herr_t{ *((std::ostringstream*)oss_)<<"  #"<<n<<" "<<err->func_name<<": "<<err->desc<<endl; return  (herr_t)0; },/*client_data*/(void*)&oss);
		throw std::runtime_error("HDF5 exception in "+e.getFuncName()+": "+e.getDetailMsg()+":\n"+oss.str());
	};
};

#endif /* WOO_HDF5 */

