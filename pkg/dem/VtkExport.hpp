#pragma once
#ifdef WOO_VTK

#include<woo/core/Engine.hpp>
#include<woo/pkg/dem/Particle.hpp>

struct Capsule; // for triangulateCapsule decl
struct Rod; // for triangulateRod decl

#pragma GCC diagnostic push
	// avoid warnings in VTK headers for using sstream
	#pragma GCC diagnostic ignored "-Wdeprecated"
	// work around error in vtkMath.h?
	__attribute__((unused))	static bool _isnan(double x){ return std::isnan(x); }
	#include<vtkCellArray.h>
	#include<vtkPoints.h>
	#include<vtkPointData.h>
	#include<vtkCellData.h>
	#include<vtkSmartPointer.h>
	#include<vtkFloatArray.h>
	#include<vtkDoubleArray.h>
	#include<vtkIntArray.h>
	#include<vtkUnstructuredGrid.h>
	#include<vtkPolyData.h>
	#include<vtkXMLUnstructuredGridWriter.h>
	#include<vtkXMLPolyDataWriter.h>
	#include<vtkZLibDataCompressor.h>
	#include<vtkTriangle.h>
	#include<vtkLine.h>
	#include<vtkXMLMultiBlockDataWriter.h>
	#include<vtkMultiBlockDataSet.h>
#pragma GCC diagnostic pop

struct VtkExport: public PeriodicEngine{
	WOO_DECL_LOGGER;
	bool acceptsField(Field* f) override { return dynamic_cast<DemField*>(f); }
	void run() override;

	enum{WHAT_SPHERES=1,WHAT_MESH=2,WHAT_STATIC=4,WHAT_TRI=8,WHAT_CON=16,WHAT_MATSTATE=32 /*,WHAT_PELLET=8*/ };
	enum{
		WHAT_ALL=           WHAT_SPHERES|WHAT_MESH|WHAT_STATIC|WHAT_TRI|WHAT_MATSTATE|WHAT_CON,
		WHAT_ALL_EXCEPT_CON=WHAT_SPHERES|WHAT_MESH|WHAT_STATIC|WHAT_TRI|WHAT_MATSTATE,
	};

	// add many vertices and vtkTriangles between them
	static int addTriangulatedObject(const vector<Vector3r>& pts, const vector<Vector3i>& tri, const vtkSmartPointer<vtkPoints>& vtkPts, const vtkSmartPointer<vtkCellArray>& cells, vector<int>& cellTypes);
	// add many vertices and vtkLines between them
	static int addLineObject(const vector<Vector3r>& pts, const vector<Vector2i>& conn, const vtkSmartPointer<vtkPoints>& vtkPts, const vtkSmartPointer<vtkCellArray>& cells, vector<int>& cellTypes);


	// TODO: convert to boost::range instead of 2 iterators
	static int triangulateStrip(const vector<int>::iterator& ABegin, const vector<int>::iterator& AEnd, const vector<int>::iterator& BBegin, const vector<int>::iterator& BEnd, bool close, vector<Vector3i>& tri);
	static int triangulateFan(const int& a, const vector<int>::iterator& BBegin, const vector<int>::iterator& BEnd, bool invert, vector<Vector3i>& tri);

	void exportMatState(const shared_ptr<MatState>& state, vector<vtkSmartPointer<vtkDoubleArray>>& matStates, size_t prevDone, Real divisor);



	static std::tuple<vector<Vector3r>,vector<Vector3i>> triangulateCapsule(const shared_ptr<Capsule>& capsule, int subdiv);
	static std::tuple<vector<Vector3r>,vector<Vector3i>> triangulateRod(const shared_ptr<Rod>& rod, int subdiv);
	static std::tuple<vector<Vector3r>,vector<Vector3i>> triangulateCapsuleLikeObject(const shared_ptr<Node>& node, const Real& rad, const Real& shaft, int subdiv);


	void postLoad(VtkExport&,void*){
		if(what>WHAT_ALL || what<0) throw std::runtime_error("VtkExport.what="+to_string(what)+", but should be at most "+to_string(WHAT_ALL)+".");
	}

	py::dict pyOutFiles() const;
	py::dict makePvdFiles() const;


	typedef map<string,vector<string>> map_string_vector_string;

	#define woo_dem_VtkExport__CLASS_BASE_DOC_ATTRS_CTOR_PY \
		VtkExport,PeriodicEngine,ClassTrait().doc("Export DEM simulation to VTK files for post-processing.").section("Export","TODO",{"FlowAnalysis","POVRayExport"}), \
		((string,out,,AttrTrait<>().buttons({"Open in Paraview","import woo.paraviewscript\nwith self.scene.paused(): woo.paraviewscript.fromVtkExport(self,launch=True)",""},/*showBefore*/true),"Filename prefix to write into; :obj:`woo.core.Scene.tags` written as {tagName} are expanded at the first run.")) \
		((bool,compress,true,,"Compress output XML files")) \
		((bool,ascii,false,,"Store data as readable text in the XML file (sets `vtkXMLWriter <http://www.vtk.org/doc/nightly/html/classvtkXMLWriter.html>`__ data mode to ``vtkXMLWriter::Ascii``, while the default is ``Appended``")) \
		((bool,multiblock,false,,"Write to multi-block VTK files, rather than separate files; currently borken, do not use.")) \
		((int,mask,0,,"If non-zero, only particles matching the mask will be exported.")) \
		((int,what,WHAT_ALL_EXCEPT_CON,AttrTrait<Attr::triggerPostLoad>(),"Select data to be saved (e.g. VtkExport.spheres|VtkExport.mesh, or use VtkExport.all for everything)")) \
		((bool,sphereSphereOnly,false,,"Only export contacts between two spheres (not sphere+facet and such)")) \
		((bool,infError,true,,"Raise exception for infinite objects which don't have the glAB attribute set properly.")) \
		((bool,skipInvisible,true,,"Skip invisible particles")) \
		((bool,savePos,false,,"Save positions of spheres (redundant information, but useful for coloring by position in Paraview.")) \
		((AlignedBox3r,clip,AlignedBox3r(),,"Only export particles of which first node is in the clip box (if given).")) \
		((int,staticMeshBit,DemField::defaultStaticBit,,"Bit for identifying static mesh particles (:obj:`Facet`, :obj:`Wall`, :obj:`InfCylinder` only) which will be exported only once.")) \
		((bool,staticMeshDone,false,,"Whether static mesh was already exported")) \
		((int,subdiv,16,AttrTrait<>(),"Subdivision fineness for circular objects (such as cylinders).\n\n.. note:: :obj:`Facets <woo.dem.Facet>` are rendered without rounded edges (they are closed flat).\n\n.. note:: :obj:`Ellipsoids <woo.dem.Ellipsoid>` triangulation is controlled via the :obj:`ellLev` parameter.")) \
		((int,ellLev,0,AttrTrait<>().range(Vector2i(0,3)),"Tesselation level for exporting ellipsoids (0 = icosahedron, each level subdivides one triangle into three.")) \
		((int,thickFacetDiv,1,AttrTrait<>(),"Subdivision for :obj:`woo.dem.Facet` objects with non-zero :obj:`woo.dem.Facet.halfThick`; the value of -1 will use :obj:`subdiv`; 0 will render only faces, without edges; 1 will close the edge flat; higher values mean the number of subdivisions.")) \
		((bool,cylCaps,true,,"Render caps of :obj:`InfCylinder` (at :obj:`InfCylinder.glAB`).")) \
		((bool,rodSurf,false,,"Export rods (and derived classes) as capsule-shaped triangulated surfaces; without this option, rods are exported as plain connecting lines.")) \
		((Real,nanValue,0.,,"Use this number instead of NaN in entries, since VTK cannot read NaNs properly")) \
		((map_string_vector_string,outFiles,,AttrTrait<>().noDump().noGui().readonly(),"Files which have been written out, keyed by what they contain: 'spheres','mesh','con'.")) \
		((vector<Real>,outTimes,,AttrTrait<>().noGui().readonly(),"Times at which files were written.")) \
		((vector<int>,outSteps,,AttrTrait<>().noGui().readonly(),"Steps at which files were written.")) \
		((bool,mkDir,false,,"Attempt to create directory for output files, if not present.")) \
		((Vector3i,prevCellNum,Vector3i::Zero(),AttrTrait<Attr::noSave>().noGui().readonly(),"Previous cell array sized, for pre-allocation.")) \
		,/*ctor*/ initRun=false; /* do not run at the very first step */ \
		,/*py*/ \
			/* this overrides the c++ map above which won't convert to python automatically */ \
			.add_property("outFiles",&VtkExport::pyOutFiles)  \
			.def("makePvdFiles",&VtkExport::makePvdFiles,"Write PVD files (one file for each category) and return dictionary mapping category name to the PVD filename; this requires that all active categories were saved at each step. Time points are output in the PVD file.")  \
			; \
			/* casting to (int) necessary, since otherwise it is a special enum type which is not registered in python and we get error: "TypeError: No to_python (by-value) converter found for C++ type: VtkExport::$_2" at boot. */ \
			_classObj.attr("spheres")=(int)VtkExport::WHAT_SPHERES; \
			_classObj.attr("mesh")=(int)VtkExport::WHAT_MESH; \
			_classObj.attr("static")=(int)VtkExport::WHAT_STATIC; \
			_classObj.attr("matState")=(int)VtkExport::WHAT_MATSTATE; \
			_classObj.attr("tri")=(int)VtkExport::WHAT_TRI; \
			_classObj.attr("con")=(int)VtkExport::WHAT_CON; \
			_classObj.attr("all")=(int)VtkExport::WHAT_ALL; \
			_classObj.attr("allExceptCon")=(int)VtkExport::WHAT_ALL_EXCEPT_CON; \
			/* _classObj.attr("pellet")=(int)VtkExport::WHAT_PELLET; */

	WOO_DECL__CLASS_BASE_DOC_ATTRS_CTOR_PY(woo_dem_VtkExport__CLASS_BASE_DOC_ATTRS_CTOR_PY);
};
WOO_REGISTER_OBJECT(VtkExport);

#endif /*WOO_VTK*/
