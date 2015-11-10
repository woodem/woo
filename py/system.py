# coding: utf-8
# 2009 © Václav Šmilauer <eudoxos@arcig.cz>

"""
Functions for accessing woo's internals; only used internally.
"""
from __future__ import print_function
import sys
from woo._customConverters import *
from woo import runtime
from woo import config
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

if 0:
    def updateScripts(scripts):
        ## Thanks goes to http://code.activestate.com/recipes/81330-single-pass-multiple-replace/
        from UserDict import UserDict
        import re,os
        class Xlator(UserDict):
            "An all-in-one multiple string substitution class; adapted to match only whole words"
            def _make_regex(self): 
                "Build a regular expression object based on the keys of the current dictionary"
                return re.compile(r"(\b%s\b)" % "|".join(self.keys()))  ## adapted here 
            def __call__(self, mo): 
                "This handler will be invoked for each regex match"
                # Count substitutions
                self.count += 1 # Look-up string
                return self[mo.string[mo.start():mo.end()]]
            def xlat(self, text):
                "Translate text, returns the modified text."
                # Reset substitution counter
                self.count = 0 
                # Process text
                return self._make_regex().sub(self, text)
        # use the _deprecated dictionary for translation, but only when matching on words boundary
        xlator=Xlator(_deprecated)
        if len(scripts)==0: print("No scripts given to --update. Nothing to do.")
        for s in scripts:
            if not s.endswith('.py'): raise RuntimeError("Refusing to do --update on file '"+s+"' (not *.py)")
            txt=open(s).read()
            txt2=xlator.xlat(txt)
            if xlator.count==0: print("%s: already up-to-date."%s)
            else:
                os.rename(s,s+'~')
                out=open(s,'w'); out.write(txt2); out.close()
                print("%s: %d subtitution%s made, backup in %s~"%(s,xlator.count,'s' if xlator.count>1 else '',s))

def releaseInternalPythonObjects():
    'Only used internally at Woo shutdown, to work around issues releasing mixed python/c++ shared_ptr. See http://stackoverflow.com/questions/33637423/pygilstate-ensure-after-py-finalize/33637604 for details.'
    import woo,woo.core,os
    if 'WOO_DEBUG' in os.environ: msg=lambda x: sys.stderr.write(x)
    else: msg=lambda x: None
    msg("Entered woo._releaseInternalPythonObjects, releasing woo.master.scene\n")
    woo.master.releaseScene() # 
    msg("woo.master.scene released, releasing attribute traits...\n")
    for c in woo.core.Object._derivedCxxClasses:
        for trait in c._attrTraits: trait._resetInternalPythonObjects()
    msg("traits released.\n")

def setExitHandlers():
    """Set exit handler to avoid gdb run if log4cxx crashes at exit."""
    # avoid backtrace at regular exit, even if we crash
    if 'log4cxx' in config.features:
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

