# File generated by kdevelop's qmake manager. 
# ------------------------------------------- 
# Subdir relative project main directory: ./libraries/yade-lib-computational-geometry
# Target is a library:  

LIBS += -lyade-lib-wm3-math 
INCLUDEPATH += $(YADEINCLUDEPATH) 
MOC_DIR = $(YADECOMPILATIONPATH) 
UI_DIR = $(YADECOMPILATIONPATH) 
OBJECTS_DIR = $(YADECOMPILATIONPATH) 
QMAKE_LIBDIR = $(YADEDYNLIBPATH)/yade-libs 
DESTDIR = $(YADEDYNLIBPATH)/yade-libs 
CONFIG += release \
          warn_on \
          dll 
TEMPLATE = lib 
HEADERS += Distances3D.hpp \
           Intersections2D.hpp \
           Intersections3D.hpp \
           MarchingCube.hpp \
           Distances2D.hpp 
SOURCES += Distances3D.cpp \
           Intersections2D.cpp \
           Intersections3D.cpp \
           MarchingCube.cpp \
           Distances2D.cpp 
