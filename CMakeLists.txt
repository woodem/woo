CMAKE_MINIMUM_REQUIRED(VERSION 3.10 FATAL_ERROR)
PROJECT(woo VERSION 1.2)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

OPTION(WOO_DEBUG "Include debugging information" ON)
OPTION(WOO_PYBIND11 "Use pybind11 instead of boost::python" ON)
OPTION(WOO_VTK "Use VTK, enable related functionality" OFF)
OPTION(WOO_HDF5 "Use HDF5, enable related functionality" OFF)
OPTION(WOO_OPENGL "Use OpenGL (requires QT5)" OFF)
OPTION(WOO_QT5 "Build Qt5-based user interface" OFF)
SET(WOO_FLAVOR "" CACHE STRING "Named configuration flavor; flavors may be installed in parallel.")
OPTION(WOO_CLANG "Use clang instead of the system-default compiler (usually gcc)" ON)

SET(PYTHON_EXECUTABLE "/usr/bin/python3" CACHE FILEPATH "The version of Python to build.")


if(WOO_CLANG)
	set(CMAKE_C_COMPILER clang-7)
	set(CMAKE_CXX_COMPILER clang++-7)
	#set(CMAKE_EXE_LINKER_FLAGS "-lz -fuse-ld=lld")
	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-lambda-capture")
else()
	# set(CMAKE_EXE_LINKER_FLAGS "-lz -fuse-ld=gold -lharfbuzz")
endif(WOO_CLANG)

#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold")

# https://stackoverflow.com/a/24305849/761090
find_program(CCACHE_FOUND "ccache")
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    # set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

# https://medium.com/@alasher/colored-c-compiler-output-with-ninja-clang-gcc-10bfe7f2b949
option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." ON)
if (${FORCE_COLORED_OUTPUT})
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
       add_compile_options (-fdiagnostics-color=always)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       add_compile_options (-fcolor-diagnostics)
    endif ()
endif ()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Eigen3 REQUIRED)
find_package(Boost REQUIRED COMPONENTS system serialization thread iostreams filesystem)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
#include(FindPython3)
#find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
find_package(PythonInterp REQUIRED)
find_package(PythonLibs REQUIRED)


find_package(VTK REQUIRED)
add_definitions(-DWOO_VTK)

if(WOO_DEBUG)
	add_definitions(-DWOO_DEBUG)
	set(CMAKE_BUILD_TYPE Debug)
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
execute_process(
	COMMAND ${PYTHON_EXECUTABLE} "-cimport site;print(site.getsitepackages()[0])"
	OUTPUT_VARIABLE WOO_MODULEDIR
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(WOO_MODULEDIR "${WOO_MODULEDIR}/woo")
message(STATUS "Python modules installed in ${WOO_MODULEDIR}")


## prepare header directories (symlink)
macro(create_symlink target linkname)
	execute_process(
		COMMAND ln -sf "${target}" "${linkname}" 
		RESULT_VARIABLE HEADER_LINK_STATUS
		ERROR_VARIABLE HEADER_LINK_ERROR
	)
	if(NOT "${HEADER_LINK_STATUS}" EQUAL 0)
		message(FATAL_ERROR "Symlinking headers failed:\n${HEADER_LINK_ERROR}")
	endif()
endmacro()
MESSAGE(STATUS "Symlinking headers …")
FILE(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/woo")
create_symlink("${CMAKE_SOURCE_DIR}/pkg" "${CMAKE_BINARY_DIR}/woo/pkg")
create_symlink("${CMAKE_SOURCE_DIR}/lib" "${CMAKE_BINARY_DIR}/woo/lib")
create_symlink("${CMAKE_SOURCE_DIR}/core" "${CMAKE_BINARY_DIR}/woo/core")


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
# add_definitions(-DWOO_GTS)

if(NOT "${WOO_FLAVOR}" STREQUAL "")
	set(WOO_MAIN_SCRIPT "woo-${WOO_FLAVOR}")
	string(REGEX REPLACE "([^a-zA-Z0-9_])" "_" WOO_CXX_FLAVOR "${WOO_FLAVOR}")
	set(CXX_INTERNAL "_cxxInternal_${WOO_CXX_FLAVOR}")
else()
	set(WOO_MAIN_SCRIPT "woo")
	set(WOO_CXX_FLAVOR "")
	set(CXX_INTERNAL "_cxxInternal")
endif()

add_definitions(-DWOO_CXX_FLAVOR=${WOO_CXX_FLAVOR})
if(WOO_DEBUG)
	set(CXX_INTERNAL "${CXX_INTERNAL}_debug")
endif()

set(WOO_CXX_SOURCES
	# python
	py/config.cxx
	py/comp.cpp
	py/_customConverters.cpp
	py/_cxxInternal.cpp
	py/log.cpp
	py/_packObb.cpp
	py/_packPredicates.cpp
	py/_packSpheres.cpp
	py/_triangulated.cpp
	py/_utils2.cpp
	py/WeightedAverage2d.cpp
	# core
	core/Cell.cpp
	core/Dispatcher.cpp
	core/DisplayParameters.cpp
	core/EnergyTracker.cpp
	core/Engine.cpp
	core/Field.cpp
	core/Functor.cpp
	core/LabelMapper.cpp
	core/Master.cpp
	core/MatchMaker.cpp
	core/Plot.cpp
	core/Preprocessor.cpp
	core/ScalarRange.cpp
	core/Scene.cpp
	core/Test.cpp
	core/Timing.cpp
	# lib
	lib/base/CompUtils.cpp
	lib/base/Math.cpp
	lib/base/Volumetric.cpp
	lib/multimethods/Indexable.cpp
	lib/object/Object.cpp
	lib/opengl/GLUtils.cpp
	lib/pyutil/except.cpp
	lib/pyutil/gil.cpp
	lib/pyutil/pickle.cpp
	lib/spdlog/src/async.cpp
	lib/spdlog/src/color_sinks.cpp
	lib/spdlog/src/file_sinks.cpp
	lib/spdlog/src/fmt.cpp
	lib/spdlog/src/spdlog.cpp
	lib/spdlog/src/stdout_sinks.cpp
	lib/sphere-pack/SpherePack.cpp
	lib/voro++/voro++.cc
	# pkg
	pkg/clDem/CLDemField.cpp
	pkg/dem/Buoyancy.cpp
	pkg/dem/Capsule.cpp
	pkg/dem/Clump.cpp
	pkg/dem/Clustering.cpp
	pkg/dem/Collision.cpp
	pkg/dem/Concrete.cpp
	pkg/dem/ContactContainer.cpp
	pkg/dem/Contact.cpp
	pkg/dem/ContactHook.cpp
	pkg/dem/ContactLoop.cpp
	pkg/dem/Conveyor.cpp
	pkg/dem/CrossAnisotropy.cpp
	pkg/dem/DynDt.cpp
	pkg/dem/Ellipsoid.cpp
	pkg/dem/Facet.cpp
	pkg/dem/FlowAnalysis.cpp
	pkg/dem/FrictMat.cpp
	pkg/dem/Funcs.cpp
	pkg/dem/G3Geom.cpp
	pkg/dem/Gl1_CPhys.cpp
	pkg/dem/Gl1_DemField.cpp
	pkg/dem/Gravity.cpp
	pkg/dem/GridBound.cpp
	pkg/dem/GridCollider.cpp
	pkg/dem/GridStore.cpp
	pkg/dem/Hdf5Export.cpp
	pkg/dem/Hertz.cpp
	pkg/dem/Ice.cpp
	pkg/dem/IdealElPl.cpp
	pkg/dem/Impose.cpp
	pkg/dem/InfCylinder.cpp
	pkg/dem/Inlet.cpp
	pkg/dem/InsertionSortCollider.cpp
	pkg/dem/IntraForce.cpp
	pkg/dem/L6Geom.cpp
	pkg/dem/LawTester.cpp
	pkg/dem/Leapfrog.cpp
	pkg/dem/Luding.cpp
	pkg/dem/MeshVolume.cpp
	pkg/dem/OpenCLCollider.cpp
	pkg/dem/Outlet.cpp
	pkg/dem/ParticleContainer.cpp
	pkg/dem/Particle.cpp
	pkg/dem/Pellet.cpp
	pkg/dem/PeriIsoCompressor.cpp
	pkg/dem/Porosity.cpp
	pkg/dem/Potential.cpp
	pkg/dem/POVRayExport.cpp
	pkg/dem/Psd.cpp
	pkg/dem/ShapePack.cpp
	pkg/dem/Sphere.cpp
	pkg/dem/SteadyState.cpp
	pkg/dem/Suspicious.cpp
	pkg/dem/Tracer.cpp
	pkg/dem/Truss.cpp
	pkg/dem/VtkExport.cpp
	pkg/dem/Wall.cpp
	pkg/fem/Membrane.cpp
	pkg/fem/Tetra.cpp
	pkg/gl/Functors.cpp
	pkg/gl/Gl1_Node.cpp
	pkg/gl/GlData.cpp
	pkg/gl/GlSetup.cpp
	pkg/gl/GlWooLogo.cpp
	pkg/gl/NodeGlRep.cpp
	pkg/gl/Renderer.cpp
	pkg/mesh/Mesh.cpp
	pkg/sparc/SparcField.cpp
	pkg/voro/VoroField.cpp
)

if(WOO_PYBIND11)
	pybind11_add_module(${CXX_INTERNAL} NO_EXTRAS
		${WOO_CXX_SOURCES}
	)
else()
	add_library(${CXX_INTERNAL} SHARED
		${WOO_CXX_SOURCES}
	)
	set_target_properties(${CXX_INTERNAL} PROPERTIES PREFIX "")
	target_link_libraries(${CXX_INTERNAL} PUBLIC Boost::python)
endif(WOO_PYBIND11)


set_source_files_properties(py/config.cxx
	PROPERTIES
	COMPILE_DEFINITIONS "WOO_REVISION=${WOO_GIT_REVISION_HASH};WOO_VERSION=${PROJECT_VERSION};WOO_SOURCE_ROOT=${CMAKE_SOURCE_DIR};WOO_BUILD_ROOT=${CMAKE_BINARY_DIR};WOO_FLAVOR=${WOO_FLAVOR};WOO_BUILD_PROGRAM=${CMAKE_MAKE_PROGRAM}"
)
target_link_libraries(${CXX_INTERNAL} PRIVATE spdlog_local)
target_link_libraries(${CXX_INTERNAL} PUBLIC
	Eigen3::Eigen
	Boost::serialization
	Boost::system
	Boost::thread
	Boost::iostreams
	Boost::boost
	Boost::filesystem
	${VTK_LIBRARIES}
)
target_include_directories(${CXX_INTERNAL} PUBLIC ${VTK_INCLUDE_DIRS})
install(TARGETS ${CXX_INTERNAL}
	LIBRARY DESTINATION ${WOO_MODULEDIR}
)


file(WRITE "${WOO_MAIN_SCRIPT}" "\#!${PYTHON_EXECUTABLE}\nimport wooMain,sys; sys.exit(wooMain.main())\n")
file(WRITE "${WOO_MAIN_SCRIPT}-batch" "\#!${PYTHON_EXECUTABLE}\nimport wooMain,sys; sys.exit(wooMain.batch())\n")


install(PROGRAMS "${WOO_MAIN_SCRIPT}" "${WOO_MAIN_SCRIPT}-batch"
	DESTINATION bin
)

install(FILES
	core/main/wooMain.py
	DESTINATION ${WOO_MODULEDIR}
)
install(FILES
	py/apiversion.py
	py/batch.py
	py/document.py
	py/eudoxos.py
	py/__init__.py
	py/ipythonintegration.py
	py/linterpolation.py
	py/manpage.py
	py/models.py
	py/objectlibrary.py
	py/pack.py
	py/paraviewscript.py
	py/plot.py
	py/post2d.py
	py/pyderived.py
	py/remote.py
	py/runtime.py
	py/system.py
	py/timing.py
	py/triangulated.py
	py/_units.py
	py/utils.py
	DESTINATION ${WOO_MODULEDIR}
)
install(FILES
	py/tests/batch.py
	py/tests/clump.py
	py/tests/clustering.py
	py/tests/core.py
	py/tests/demfield.py
	py/tests/ellipsoid.py
	py/tests/energy.py
	py/tests/grid.py
	py/tests/hertz.py
	py/tests/__init__.py
	py/tests/io.py
	py/tests/labels.py
	py/tests/pbc.py
	py/tests/psd.py
	py/tests/shapepack.py
	py/tests/tetra.py
	py/tests/volumetric.py
	DESTINATION ${WOO_MODULEDIR}/tests
)
install(FILES
	py/pre/chute.py
	py/pre/cylTriax.py
	py/pre/depot.py
	py/pre/ell2d.py
	py/pre/horse.py
	py/pre/__init__.py
	py/pre/psdrender.py
	py/pre/staticmixer.py
	py/pre/toys.py
	py/pre/triax.py
	DESTINATION ${WOO_MODULEDIR}/pre
)
install(FILES
	py/_monkey/aliases.py
	py/_monkey/compat.py
	py/_monkey/extraDocs.py
	py/_monkey/gts.py
	py/_monkey/__init__.py
	py/_monkey/io.py
	DESTINATION ${WOO_MODULEDIR}/_monkey
)
install(FILES
	py/data/horse.coarse.gts
	py/data/horse.coarse.stl
	py/data/horse.nmesh
	py/data/ng.ini
	py/data/ngmesh.ini
	py/data/ship.stl
	py/data/woodem-6.small.svg
	py/data/woodem-6.woodem.svg
	py/data/woodem-6.woo.svg
	py/data/woo-favicon.ico
	py/data/woo-icon.128.png
	py/data/woo-icon.256.png
	DESTINATION ${WOO_MODULEDIR}/data
)


##
## TODO: run setup.py for wooExtra modules
##
