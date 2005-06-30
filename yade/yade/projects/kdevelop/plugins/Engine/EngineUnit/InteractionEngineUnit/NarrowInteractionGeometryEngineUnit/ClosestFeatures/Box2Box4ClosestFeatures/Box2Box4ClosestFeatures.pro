# File generated by kdevelop's qmake manager. 
# ------------------------------------------- 
# Subdir relative project main directory: ./plugins/Engine/EngineUnit/InteractionEngineUnit/NarrowInteractionGeometryEngineUnit/ClosestFeatures/Box2Box4ClosestFeatures
# Target is a library:  

LIBS += -lyade-lib-wm3-math \
        -lyade-lib-multimethods \
        -lyade-lib-computational-geometry \
        -lInteractingBox \
        -lInteractionGeometryMetaEngine \
        -lBox \
        -lClosestFeatures \
        -rdynamic 
INCLUDEPATH += $(YADEINCLUDEPATH) 
MOC_DIR = $(YADECOMPILATIONPATH) 
UI_DIR = $(YADECOMPILATIONPATH) 
OBJECTS_DIR = $(YADECOMPILATIONPATH) 
QMAKE_LIBDIR = ../../../../../../../libraries/yade-lib-wm3-math/$(YADEDYNLIBPATH)/yade-libs \
               ../../../../../../../libraries/yade-lib-multimethods/$(YADEDYNLIBPATH)/yade-libs \
               ../../../../../../../libraries/yade-lib-computational-geometry/$(YADEDYNLIBPATH)/yade-libs \
               ../../../../../../../plugins/Data/Body/InteractingGeometry/InteractingBox/$(YADEDYNLIBPATH) \
               ../../../../../../../plugins/Engine/MetaEngine/InteractionMetaEngine/NarrowInteractionGeometryMetaEngine/InteractionGeometryMetaEngine/$(YADEDYNLIBPATH) \
               $(YADEDYNLIBPATH)/yade-libs \
               $(YADEDYNLIBPATH) 
QMAKE_CXXFLAGS_RELEASE += -lpthread \
                          -pthread 
QMAKE_CXXFLAGS_DEBUG += -lpthread \
                        -pthread 
DESTDIR = $(YADEDYNLIBPATH) 
CONFIG += debug \
          warn_on \
          dll 
TEMPLATE = lib 
HEADERS += Box2Box4ClosestFeatures.hpp 
SOURCES += Box2Box4ClosestFeatures.cpp 
