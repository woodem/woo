# coding: utf-8
# 2009 © Václav Šmilauer <eudoxos@arcig.cz>

"""
Functions for accessing woo's internals; only used internally.
"""
from __future__ import print_function
import sys
from woo import config
from woo._customConverters import *
from woo import runtime
import woo.core

def childClasses(base,recurse=True,includeBase=False):
    """Enumerate classes deriving from given base (as string), recursively by default. Returns set."""
    if isinstance(base,str): ret=set(woo.master.childClassesNonrecursive(base));
    else: ret=set(base.__subclasses__())
    ret2=set();
    if includeBase: ret|=set([base])
    if not recurse: return ret
    for bb in ret:
        ret2|=childClasses(bb)
    return ret | ret2

_allSerializables=[c.__name__ for c in woo.core.Object._derivedCxxClasses]
## set of classes for which the proxies were created
_proxiedClasses=set()

def releaseInternalPythonObjects():
    'Only used internally at Woo shutdown, to work around issues releasing mixed python/c++ shared_ptr. See http://stackoverflow.com/questions/33637423/pygilstate-ensure-after-py-finalize/33637604 for details.'
    try: import woo.core,os
    except ImportError: return
    if 'WOO_DEBUG' in os.environ: msg=lambda x: sys.stderr.write(x)
    else: msg=lambda x: None
    msg("Entered woo._releaseInternalPythonObjects, releasing woo.master.scene\n")
    woo.master.releaseScene() # 
    msg("woo.master.scene released, releasing attribute traits...\n")
    for c in woo.core.Object._derivedCxxClasses:
        for trait in c._attrTraits: trait._resetInternalPythonObjects()
    if woo.master.scene: woo.master.scene.plot._resetInternalPythonObjects()
    msg("traits released.\n")

def setExitHandlers():
    """Set exit handler to avoid gdb run if log4cxx crashes at exit."""
    # avoid backtrace at regular exit, even if we crash

    if False and 'log4cxx' in config.features:
        __builtins__['quit']=woo.master.exitNoBacktrace
        sys.exit=woo.master.exitNoBacktrace
    # this seems to be not needed anymore:
    #sys.excepthook=sys.__excepthook__ # apport on ubuntu overrides this, we don't need it

    import atexit
    atexit.register(releaseInternalPythonObjects)



# consistency check
# if there are no serializables, then plugins were not loaded yet, probably
if(len(_allSerializables)==0):
    raise ImportError("No classes deriving from Object found; you must call woo.boot.initialize to load plugins before importing woo.system.")

