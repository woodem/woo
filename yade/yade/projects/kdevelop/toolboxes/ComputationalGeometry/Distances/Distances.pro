# File generated by kdevelop's qmake manager. 
# ------------------------------------------- 
# Subdir relative project main directory: ./toolboxes/ComputationalGeometry/Distances
# Target is a library:  

LIBS += $(YADECOMPILATIONPATH)/libBody.a \
        $(YADECOMPILATIONPATH)/libEngine.a \
        $(YADECOMPILATIONPATH)/libGeometry.a \
        $(YADECOMPILATIONPATH)/libInteraction.a \
        $(YADECOMPILATIONPATH)/libMultiMethods.a \
        $(YADECOMPILATIONPATH)/libFactory.a \
        $(YADECOMPILATIONPATH)/libSerialization.a \
	-rdynamic 
INCLUDEPATH = ../../../yade/yade \
              ../../../yade/Factory \
              ../../../yade/Serialization \
              ../../../toolboxes/Math 
MOC_DIR = $(YADECOMPILATIONPATH) 
UI_DIR = $(YADECOMPILATIONPATH) 
OBJECTS_DIR = $(YADECOMPILATIONPATH) 
QMAKE_LIBDIR = $(YADEDYNLIBPATH) 
DESTDIR = $(YADEDYNLIBPATH) 
CONFIG += debug \
          warn_on \
          dll 
TEMPLATE = lib 
HEADERS += Distances3D.hpp 
SOURCES += Distances3D.cpp 
