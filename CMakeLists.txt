CMAKE_MINIMUM_REQUIRED(VERSION 3.10 FATAL_ERROR)
PROJECT(woo VERSION 1.2)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

OPTION(WOO_DEBUG "Include debugging information" OFF)
OPTION(WOO_VTK "Use VTK, enable related functionality" ON)
OPTION(WOO_HDF5 "Use HDF5, enable related functionality" ON)
OPTION(WOO_GTS "Use GTS (and build internal pygts), enable related functionality" ON)
OPTION(WOO_QT5 "Build Qt5-based user interface (implies also OpenGL)" ON)
OPTION(WOO_OPENMP "Enable parallel computing based on OpenMP" ON)
SET(WOO_FLAVOR "" CACHE STRING "Named configuration flavor; flavors may be installed in parallel.")
SET(WOO_INSTALL_SCHEME "posix_user" CACHE STRING "Python installation scheme, see https://docs.python.org/3/library/sysconfig.html#installation-paths")
SET(WOO_BUILD_JOBS 0 CACHE STRING "Set default number of parallel build level, when building with woo -R; 0 is to use the default (all cores)")
OPTION(WOO_CCACHE "Use ccache when compiling" ON)
OPTION(WOO_CEREAL "Use cereal instead of boost::serialization (experimental!)" OFF)
SET(PYTHON_EXECUTABLE "/usr/bin/python3" CACHE FILEPATH "The version of Python to build.")

if(DEFINED ENV{VIRTUAL_ENV})
	if("${WOO_INSTALL_SCHEME}" STREQUAL "posix_user")
		message(STATUS "Setting WOO_INSTALL_SCHEME to posix_prefix (in virtual environment)")
		set(WOO_INSTALL_SCHEME "posix_prefix")
	endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKED_FLAGS} -fuse-ld=gold")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	# https://github.com/pybind/pybind11/issues/1604#issuecomment-459796241
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsized-deallocation")
endif()

# https://stackoverflow.com/a/24305849/761090
if(WOO_CCACHE)
	find_program(CCACHE_FOUND "ccache" REQUIRED)
	set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
endif(WOO_CCACHE)

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
set(CMAKE_INSTALL_MESSAGE LAZY)

find_package(Eigen3 REQUIRED)
find_package(Boost REQUIRED COMPONENTS serialization iostreams system)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
#include(FindPython3)
#find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
# set(Python_ADDITIONAL_VERSIONS 3.6 3.7 3.8)
find_package(PythonInterp REQUIRED)
find_package(PythonLibs REQUIRED)
if("${PYTHON_VERSION_MAJOR}" STREQUAL "2")
	message(FATAL_ERROR "Python 2.x is not supported.")
endif()

set(Backward_DIR ${PROJECT_SOURCE_DIR}/lib/backward/cmake)
find_package(Backward REQUIRED)

if(WOO_DEBUG)
	add_definitions(-DWOO_DEBUG)
	set(CMAKE_BUILD_TYPE Debug)
else()
	add_definitions(-DNDEBUG)
	set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(pybind11 2.4 REQUIRED)

find_path(DOUBLECONVERSION_INCLUDE_DIR NAMES double-conversion/double-conversion.h PATHS /usr/include /usr/local/include)
find_library(DOUBLECONVERSION_LIB NAMES double-conversion)
if(DOUBLECONVERSION_INCLUDE_DIR AND DOUBLECONVERSION_LIB)
	# create interface: https://foonathan.net/2016/07/cmake-dependency-handling/
	add_library(DOUBLECONVERSION INTERFACE)
	target_include_directories(DOUBLECONVERSION INTERFACE ${DOUBLECONVERSION_INCLUDE_DIR})
	target_link_libraries(DOUBLECONVERSION INTERFACE ${DOUBLECONVERSION_LIB})
else()
	message(FATAL_ERROR "double-conversion library/headers not found.")
endif()

# for parallel STL algotithms
find_package(TBB REQUIRED)

if(WOO_CEREAL)
	# requires -DWOO_NOXML but that is set by default already
	add_definitions(-DWOO_CEREAL)
	# TODO: check headers and version
endif()

if(WOO_OPENMP)
	add_definitions(-DWOO_OPENMP)
	find_package(OpenMP REQUIRED)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

if(WOO_GTS)
	add_definitions(-DWOO_GTS)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTS REQUIRED gts)
	# create interface: https://foonathan.net/2016/07/cmake-dependency-handling/
	add_library(GTS INTERFACE)
	target_include_directories(GTS INTERFACE ${GTS_INCLUDE_DIRS})
	target_link_libraries(GTS INTERFACE ${GTS_LIBRARIES})
	set(WOO_CXX_GTS_SOURCES
		py/3rd-party/pygts-0.3.1/cleanup.cpp
		py/3rd-party/pygts-0.3.1/edge.cpp
		py/3rd-party/pygts-0.3.1/face.cpp
		py/3rd-party/pygts-0.3.1/object.cpp
		py/3rd-party/pygts-0.3.1/point.cpp
		py/3rd-party/pygts-0.3.1/pygts.cpp
		py/3rd-party/pygts-0.3.1/segment.cpp
		py/3rd-party/pygts-0.3.1/surface.cpp
		py/3rd-party/pygts-0.3.1/triangle.cpp
		py/3rd-party/pygts-0.3.1/vertex.cpp
	)
	add_definitions(-DPYGTS_HAS_NUMPY)
endif()

if(WOO_VTK)
	find_package(VTK REQUIRED)
	add_definitions(-DWOO_VTK)
endif()

if(WOO_HDF5)
	find_package(HDF5 REQUIRED COMPONENTS CXX HL)
	add_definitions(-DWOO_HDF5)
	set(HDF5_PREFER_PARALLEL false)
	include_directories(SYSTEM PUBLIC ${HDF5_CXX_INCLUDE_DIRS})
endif()

if(WOO_QT5)
	find_package(Qt5 COMPONENTS Widgets Core OpenGL Xml REQUIRED)
	add_definitions(-DWOO_QT5 -DWOO_OPENGL)
	set(CMAKE_AUTOMOC ON)
	set(CMAKE_AUTOUIC ON)
	set(CMAKE_AUTORCC ON)
	set(OpenGL_GL_PREFERENCE GLVND)
	find_package(OpenGL REQUIRED)
	find_package(GLUT REQUIRED)
	find_package(GLEW REQUIRED)
	# GLE
	find_path(GLE_INCLUDE_DIR NAMES GL/gle.h PATHS /usr/include /usr/local/include)
	find_library(GLE_LIB NAMES gle)
	if(GLE_INCLUDE_DIR AND GLE_LIB)
		# create interface: https://foonathan.net/2016/07/cmake-dependency-handling/
		add_library(GLE INTERFACE)
		target_include_directories(GLE INTERFACE ${GLE_INCLUDE_DIR})
		target_link_libraries(GLE INTERFACE ${GLE_LIB})
	else()
		message(FATAL_ERROR "GLE library/headers not found.")
	endif()
	# QGLViewer
	find_path(QGLVIEWER_INCLUDE_DIR NAMES qglviewer.h PATHS /usr/include/QGLViewer /usr/local/include/QGLViewer /usr/include/qglviewer /usr/local/include/qglviewer)
	find_library(QGLVIEWER_LIB NAMES QGLViewer QGLViewer-qt5 qglviewer-qt5)
	if(QGLVIEWER_INCLUDE_DIR AND QGLVIEWER_LIB)
		add_library(QGLViewer INTERFACE)
		target_include_directories(QGLViewer INTERFACE ${QGLVIEWER_INCLUDE_DIR})
		target_link_libraries(QGLViewer INTERFACE ${QGLVIEWER_LIB})
	else()
		message(FATAL_ERROR "QGLViewer library/headers not found (headers: ${QGLVIEWER_INCLUDE_DIR}, lib: ${QGLVIEWER_LIBS}).")
	endif()
	set(WOO_CXX_GUI_SOURCES
		gui/qt4/GLViewer.qrc
		gui/qt4/GLViewer.cpp
		gui/qt4/_GLViewer.cpp
		gui/qt4/OpenGLManager.cpp
	)
endif(WOO_QT5)

# don't compile XML serialization, binary works better
add_definitions(-DWOO_NOXML)

execute_process(COMMAND git log -1 --format=%h
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	OUTPUT_VARIABLE WOO_GIT_REVISION_HASH
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
	COMMAND ${PYTHON_EXECUTABLE} "-cimport sysconfig;print(sysconfig.get_path('platlib','${WOO_INSTALL_SCHEME}'))"
	OUTPUT_VARIABLE WOO_TOP_MODULEDIR
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(WOO_MODULEDIR "${WOO_TOP_MODULEDIR}/woo")
message(STATUS "Python modules to be installed in ${WOO_MODULEDIR} (scheme: ${WOO_INSTALL_SCHEME})")
execute_process(
	COMMAND ${PYTHON_EXECUTABLE} "-cimport sysconfig;print(sysconfig.get_path('scripts','${WOO_INSTALL_SCHEME}'))"
	OUTPUT_VARIABLE WOO_BIN_DIR
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Executables to be installed in ${WOO_BIN_DIR} (scheme: ${WOO_INSTALL_SCHEME})")

## prepare header directories (symlink)
macro(create_symlink target linkname)
	message(STATUS "Symlinking ${linkname} → ${target}")
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
FILE(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include/woo")
create_symlink("${CMAKE_SOURCE_DIR}/pkg" "${CMAKE_BINARY_DIR}/include/woo")
create_symlink("${CMAKE_SOURCE_DIR}/lib" "${CMAKE_BINARY_DIR}/include/woo")
create_symlink("${CMAKE_SOURCE_DIR}/core" "${CMAKE_BINARY_DIR}/include/woo")


add_definitions(-DWOO_SPDLOG)

if(NOT "${WOO_FLAVOR}" STREQUAL "")
	set(WOO_MAIN_SCRIPT "woo-${WOO_FLAVOR}")
	string(REGEX REPLACE "([^a-zA-Z0-9_])" "_" WOO_CXX_FLAVOR "_${WOO_FLAVOR}")
	set(CXX_INTERNAL "_cxxInternal${WOO_CXX_FLAVOR}")
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
	lib/backward/backward.cpp
	lib/base/CompUtils.cpp
	lib/base/Math.cpp
	lib/base/Volumetric.cpp
	lib/multimethods/Indexable.cpp
	lib/object/Object.cpp
	lib/opengl/GLUtils.cpp
	lib/pyutil/except.cpp
	lib/pyutil/gil.cpp
	lib/pyutil/pickle.cpp
	lib/sphere-pack/SpherePack.cpp
	lib/voro++/voro++.cc
	lib/eigen/pybind11/register.cpp
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


set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

message(STATUS "All sources are ${WOO_CXX_SOURCES};;")
INCLUDE(wooCombineSources)
COMBINE_SOURCES("${CMAKE_BINARY_DIR}/srcs" ;
	"${WOO_CXX_SOURCES}" ;
	1 ;
	WOO_CXX_SOURCES_COMBINED
)
message(STATUS "Combined sources are ${WOO_CXX_SOURCES_COMBINED}")

pybind11_add_module(${CXX_INTERNAL} NO_EXTRAS
	${WOO_CXX_SOURCES_COMBINED}
	${WOO_CXX_GUI_SOURCES}
	${WOO_CXX_GTS_SOURCES}
)
target_link_libraries(${CXX_INTERNAL} PUBLIC DOUBLECONVERSION tbb)

target_include_directories(${CXX_INTERNAL} PRIVATE ${CMAKE_BINARY_DIR}/include)
if(WOO_QT5)
	target_link_libraries(${CXX_INTERNAL} PUBLIC Qt5::Core Qt5::Widgets Qt5::OpenGL Qt5::Xml OpenGL::GL GLUT::GLUT GLEW::GLEW QGLViewer GLE)
endif(WOO_QT5)
if(WOO_OPENMP)
	target_link_libraries(${CXX_INTERNAL} PUBLIC OpenMP::OpenMP_CXX)
endif(WOO_OPENMP)

if(WOO_GTS)
	target_link_libraries(${CXX_INTERNAL} PUBLIC GTS)
endif(WOO_GTS)

if(WOO_HDF5)
	target_link_libraries(${CXX_INTERNAL} PUBLIC ${HDF5_CXX_LIBRARIES} ${HDF5_HL_LIBRARIES})
endif(WOO_HDF5)

set_source_files_properties(py/config.cxx
	PROPERTIES
	COMPILE_DEFINITIONS "WOO_REVISION=${WOO_GIT_REVISION_HASH};WOO_VERSION=${PROJECT_VERSION};WOO_SOURCE_ROOT=${CMAKE_SOURCE_DIR};WOO_BUILD_ROOT=${CMAKE_BINARY_DIR};WOO_FLAVOR=${WOO_FLAVOR};WOO_BUILD_PROGRAM=${CMAKE_MAKE_PROGRAM};WOO_BUILD_JOBS=${WOO_BUILD_JOBS}"
)
target_link_libraries(${CXX_INTERNAL} PRIVATE Backward::Backward)


OPTION(SPDLOG_LOCAL "build using local spdlog version" OFF)
if(SPDLOG_LOCAL)
	add_library(spdlog_local STATIC
		lib/spdlog/src/async.cpp
		lib/spdlog/src/color_sinks.cpp
		lib/spdlog/src/file_sinks.cpp
		lib/spdlog/src/fmt.cpp
		lib/spdlog/src/spdlog.cpp
		lib/spdlog/src/stdout_sinks.cpp
	)
	target_include_directories(spdlog_local PUBLIC lib/)
	target_link_libraries(${CXX_INTERNAL} PRIVATE spdlog_local)
else()
	find_package(spdlog REQUIRED 1.5)
	target_link_libraries(${CXX_INTERNAL} PRIVATE spdlog::spdlog)
endif()


target_link_libraries(${CXX_INTERNAL} PUBLIC
	Eigen3::Eigen
	Boost::serialization
	Boost::iostreams
	Boost::boost
	Boost::system
	stdc++fs
	${VTK_LIBRARIES}
)
target_include_directories(${CXX_INTERNAL} SYSTEM PUBLIC ${VTK_INCLUDE_DIRS})
install(TARGETS ${CXX_INTERNAL}
	LIBRARY DESTINATION ${WOO_MODULEDIR}
)

FILE(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
file(WRITE "${CMAKE_BINARY_DIR}/bin/${WOO_MAIN_SCRIPT}" "\#!${PYTHON_EXECUTABLE}\nimport wooMain,sys; sys.exit(wooMain.main())\n")
file(WRITE "${CMAKE_BINARY_DIR}/bin/${WOO_MAIN_SCRIPT}-batch" "\#!${PYTHON_EXECUTABLE}\nimport wooMain,sys; sys.exit(wooMain.batch())\n")


install(PROGRAMS "${CMAKE_BINARY_DIR}/bin/${WOO_MAIN_SCRIPT}" "${CMAKE_BINARY_DIR}/bin/${WOO_MAIN_SCRIPT}-batch"
	DESTINATION ${WOO_BIN_DIR}
)

install(FILES
	core/main/wooMain.py
	DESTINATION ${WOO_TOP_MODULEDIR}
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
	py/data/woodem-6.small.svg
	py/data/woodem-6.woodem.svg
	py/data/woodem-6.woo.svg
	py/data/woo-favicon.ico
	py/data/woo-icon.128.png
	py/data/woo-icon.256.png
	DESTINATION ${WOO_MODULEDIR}/data
)

if(WOO_GTS)
	install(FILES
		py/3rd-party/pygts-0.3.1/__init__.py
		py/3rd-party/pygts-0.3.1/pygts.py
		DESTINATION ${WOO_MODULEDIR}/gts
	)
endif(WOO_GTS)

if(WOO_QT5)
	macro(PYQT_QRC SRC DST)
		add_custom_target("${DST}" ALL
		#add_custom_command(OUTPUT "${DST}" ALL
			COMMAND pyrcc5 -o ${DST} ${SRC}
			DEPENDS ${SRC}
			VERBATIM
		)
	endmacro()
	macro(PYQT_UI SRC DST)
		add_custom_target("${DST}" ALL
		#add_custom_command(OUTPUT "${DST}" ALL
			COMMAND pyuic5 --from-imports -o ${DST} ${SRC}
			DEPENDS ${SRC}
			VERBATIM
		)
	endmacro()
	PYQT_QRC(${CMAKE_SOURCE_DIR}/gui/qt4/img.qrc img_rc.py)
	PYQT_UI(${CMAKE_SOURCE_DIR}/gui/qt4/controller.ui ui_controller.py)
	#add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/ui_controller.py
	#	COMMAND pyuic5 --from-imports -o ${CMAKE_BINARY_DIR}/ui_controller.py gui/qt4/controller.ui
	#	DEPENDS gui/qt4/controller.ui
	#	VERBATIM
	#)
	install(FILES
		${CMAKE_BINARY_DIR}/img_rc.py
		${CMAKE_BINARY_DIR}/ui_controller.py
		gui/qt4/DisplayProfiles.py
		gui/qt4/ExceptionDialog.py
		gui/qt4/__init__.py
		gui/qt4/Inspector.py
		gui/qt4/ObjectEditor.py
		DESTINATION ${WOO_MODULEDIR}/qt
	)
endif()

##
## run setup.py for wooExtra modules
##
configure_file(wooExtra.cmake ${CMAKE_BINARY_DIR}/wooExtra.cmake @ONLY)
install(SCRIPT ${CMAKE_BINARY_DIR}/wooExtra.cmake)
