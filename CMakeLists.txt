CMAKE_MINIMUM_REQUIRED(VERSION 2.8 FATAL_ERROR)
PROJECT(woo)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

OPTION(WOO_DEBUG "Include debugging information" OFF)
OPTION(WOO_PYBIND11 "Use pybind11 instead of boost::python" ON)
OPTION(WOO_VTK "Use VTK, enable related functionality" OFF)
OPTION(WOO_HDF5 "Use HDF5, enable related functionality" OFF)
OPTION(WOO_OPENGL "Use OpenGL (requires QT5)" OFF)
OPTION(WOO_QT5 "Build Qt5-based user interface" OFF)
OPTION(WOO_FLAVOR "Named configuration flavor; flavors may be installed in parallel." "")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold")

# https://stackoverflow.com/a/24305849/761090
find_program(CCACHE_FOUND "ccache")
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    # set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Eigen3 REQUIRED)
find_package(Boost REQUIRED COMPONENTS system serialization thread iostreams filesystem)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(PythonLibs ${PY_VERSION} REQUIRED)

if(WOO_DEBUG)
	add_definitions(-DWOO_DEBUG)
endif()
if(WOO_PYBIND11)
	add_definitions(-DWOO_PYBIND11)
	find_package(pybind11 REQUIRED)
else()
	find_package(Boost REQUIRED COMPONENTS python)
endif()
if(WOO_VTK)
	find_package(VTK REQUIRED)
	add_definitions(-DWOO_VTK)
endif()
if(WOO_HDF5)
	find_package(HDF5 REQUIRED COMPONENTS CXX HL)
	add_definitions(-DWOO_HDF5)
	set(HDF5_PREFER_PARALLEL false)
endif()
if(WOO_OPENGL)
	find_package(GL REQUIRED)
	find_package(GLU REQUIRED)
	find_package(glut REQUIRED)
	find_package(gle REQUIRED)
	find_package(qglviewer REQUIRED)
	add_definitions(-DWOO_OPENGL)
endif()
if(WOO_QT5)
	find_package(Qt5 COMPONENTS QtGui QtWidgets QtCore QtOpenGL REQUIRED)
	add_definitions(-DWOO_QT5)
	set(CMAKE_AUTOMOC ON)
	set(CMAKE_AUTOUIC ON)
	set(CMAKE_AUTORCC ON)
endif()
if(WOO_OPENMP)
	find_package(OpenMP REQUIRED)
	add_definitions(-DWOO_OPENMP)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

execute_process(COMMAND git log -1 --format=%h
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	OUTPUT_VARIABLE WOO_GIT_REVISION_HASH
	OUTPUT_STRIP_TRAILING_WHITESPACE
)


add_library(spdlog_local STATIC
	lib/spdlog/src/async.cpp
	lib/spdlog/src/color_sinks.cpp
	lib/spdlog/src/file_sinks.cpp
	lib/spdlog/src/fmt.cpp
	lib/spdlog/src/spdlog.cpp
	lib/spdlog/src/stdout_sinks.cpp
)
target_include_directories(spdlog_local PUBLIC lib/)
target_compile_definitions(spdlog_local PUBLIC SPDLOG_COMPILED_LIB)
add_definitions(-DWOO_SPDLOG)

if(WOO_FLAVOR)
	string(REGEX_REPLACE "([^a-zA-Z0-9_])" "_" WOO_FLAVOR "${WOO_CXX_FLAVOR}")
	set(CXX_INTERNAL "_cxxInternal_${WOO_CXX_FLAVOR}")
else()
	set(WOO_CXX_FLAVOR "")
	set(CXX_INTERNAL "_cxxInternal")
endif()
add_definitions(-DWOO_CXX_FLAVOR=${WOO_CXX_FLAVOR})
if(WOO_DEBUG)
	set(CXX_INTERNAL "${CXX_INTERNAL}_debug")
endif()

pybind11_add_module(${CXX_INTERNAL} NO_EXTRAS
	# python
	py/comp.cpp  py/_customConverters.cpp  py/_cxxInternal.cpp  py/log.cpp  py/_packObb.cpp  py/_packPredicates.cpp  py/_packSpheres.cpp  py/_triangulated.cpp  py/_utils2.cpp  py/WeightedAverage2d.cpp
	# core
	core/Cell.cpp        core/DisplayParameters.cpp  core/Engine.cpp  core/Functor.cpp      core/Master.cpp      core/Plot.cpp          core/ScalarRange.cpp  core/Test.cpp
	core/Dispatcher.cpp  core/EnergyTracker.cpp      core/Field.cpp   core/LabelMapper.cpp  core/MatchMaker.cpp  core/Preprocessor.cpp  core/Scene.cpp        core/Timing.cpp
	# lib
	lib/base/CompUtils.cpp          lib/opengl/GLUtils.cpp    lib/spdlog/src/color_sinks.cpp   lib/sphere-pack/SpherePack.cpp  
	lib/base/Math.cpp               lib/pyutil/except.cpp     lib/spdlog/src/file_sinks.cpp    
	lib/base/Volumetric.cpp         lib/pyutil/gil.cpp        lib/spdlog/src/fmt.cpp           lib/voro++/voro++.cc
	lib/multimethods/Indexable.cpp  lib/pyutil/pickle.cpp     lib/spdlog/src/spdlog.cpp        
	lib/object/Object.cpp           lib/spdlog/src/async.cpp  lib/spdlog/src/stdout_sinks.cpp  
	# pkg
	pkg/clDem/CLDemField.cpp      pkg/dem/ContactLoop.cpp      pkg/dem/Gl1_CPhys.cpp     pkg/dem/Impose.cpp                 pkg/dem/OpenCLCollider.cpp     pkg/dem/ShapePack.cpp    pkg/gl/Functors.cpp
	pkg/dem/Buoyancy.cpp          pkg/dem/Conveyor.cpp         pkg/dem/Gl1_DemField.cpp  pkg/dem/InfCylinder.cpp            pkg/dem/Outlet.cpp             pkg/dem/Sphere.cpp       pkg/gl/Gl1_Node.cpp
	pkg/dem/Capsule.cpp           pkg/dem/CrossAnisotropy.cpp  pkg/dem/Gravity.cpp       pkg/dem/Inlet.cpp                  pkg/dem/ParticleContainer.cpp  pkg/dem/SteadyState.cpp  pkg/gl/GlData.cpp
	pkg/dem/Clump.cpp             pkg/dem/DynDt.cpp            pkg/dem/GridBound.cpp     pkg/dem/InsertionSortCollider.cpp  pkg/dem/Particle.cpp           pkg/dem/Suspicious.cpp   pkg/gl/GlSetup.cpp
	pkg/dem/Clustering.cpp        pkg/dem/Ellipsoid.cpp        pkg/dem/GridCollider.cpp  pkg/dem/IntraForce.cpp             pkg/dem/Pellet.cpp             pkg/dem/Tracer.cpp       pkg/gl/GlWooLogo.cpp
	pkg/dem/Collision.cpp         pkg/dem/Facet.cpp            pkg/dem/GridStore.cpp     pkg/dem/L6Geom.cpp                 pkg/dem/PeriIsoCompressor.cpp  pkg/dem/Truss.cpp        pkg/gl/NodeGlRep.cpp
	pkg/dem/Concrete.cpp          pkg/dem/FlowAnalysis.cpp     pkg/dem/Hdf5Export.cpp    pkg/dem/LawTester.cpp              pkg/dem/Porosity.cpp           pkg/dem/VtkExport.cpp    pkg/gl/Renderer.cpp
	pkg/dem/ContactContainer.cpp  pkg/dem/FrictMat.cpp         pkg/dem/Hertz.cpp         pkg/dem/Leapfrog.cpp               pkg/dem/Potential.cpp          pkg/dem/Wall.cpp         pkg/mesh/Mesh.cpp
	pkg/dem/Contact.cpp           pkg/dem/Funcs.cpp            pkg/dem/Ice.cpp           pkg/dem/Luding.cpp                 pkg/dem/POVRayExport.cpp       pkg/fem/Membrane.cpp     pkg/sparc/SparcField.cpp
	pkg/dem/ContactHook.cpp       pkg/dem/G3Geom.cpp           pkg/dem/IdealElPl.cpp     pkg/dem/MeshVolume.cpp             pkg/dem/Psd.cpp                pkg/fem/Tetra.cpp        pkg/voro/VoroField.cpp
)
	# src/carxxpy.cpp
	# src/other/pybind11_opencv_numpy/ndarray_converter.cpp)
target_link_libraries(${CXX_INTERNAL} PRIVATE spdlog_local)
target_link_libraries(${CXX_INTERNAL} PUBLIC Eigen3::Eigen Boost::serialization Boost::system Boost::thread Boost::iostreams Boost::boost Boost::filesystem)

macro(create_symlink src dest target)
  add_custom_command(TARGET ${target} PRE_BUILD COMMAND ln -sf ${src} ${dest} COMMENT "linking ${src} → ${dest}" VERBATIM)
endmacro()

FILE(MAKE_DIRECTORY "woo")
FILE(COPY "pkg" DESTINATION "woo" FILES_MATCHING PATTERN "*.hpp")
FILE(COPY "lib" DESTINATION "woo" FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.hh")
FILE(COPY "core" DESTINATION "woo" FILES_MATCHING PATTERN "*.hpp")
#create_symlink("woo/pkg" "${CMAKE_SOURCE_DIR}/pkg" "${CXX_INTERNAL}")
#create_symlink("woo/lib" "${CMAKE_SOURCE_DIR}/lib" "${CXX_INTERNAL}")
#create_symlink("woo/core" "${CMAKE_SOURCE_DIR}/core" "${CXX_INTERNAL}")
#FILE(CREATE_LINK 
