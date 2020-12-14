# this module is populated at initialization from the c++ part of PythonUI
"""Runtime variables, populated at woo startup."""
from __future__ import print_function
# default value
import wooMain
hasDisplay=None
if wooMain.options.fakeDisplay: hasDisplay=False # we would crash really

# cache the value returned
_ipython_version=None

def ipython_version():
    # find out about which ipython version we use -- 0.10* and 0.11 are supported, but they have different internals
    global _ipython_version
    if not _ipython_version==None: return _ipython_version
    # workaround around bug in IPython 0.13 (and perhaps later): frozen install does not define sys.argv
    import sys
    fakedArgv=False
    if hasattr(sys,'frozen') and len(sys.argv)==0:
        sys.argv=['wwoo']
        fakedArgv=True
    if len(sys.argv)==0: ## ???
        sys.argv=['woo']
        fakedArgv=True
    import IPython
    try: # attempt to get numerical version
        if IPython.__version__.startswith('0.'): raise RuntimeError('IPython 0.x is not supported anymore.')
        elif IPython.__version__.startswith('1.'): raise RuntimeError('IPython 1.x is not supported anymore.')
        elif IPython.__version__.startswith('2.'): raise RuntimeError('IPython 2.x is not supported anymore.')
        elif IPython.__version__.startswith('3.'):  ret=300
        elif IPython.__version__.startswith('4.'):  ret=400
        elif IPython.__version__.startswith('5.'):  ret=500
        elif IPython.__version__.startswith('6.'):  ret=600
        elif IPython.__version__.startswith('7.'):  ret=700
        elif IPython.__version__.startswith('8.'):  ret=800
        elif IPython.__version__.startswith('9.'):  ret=900
        else: raise ValueError() # nothing detected, issue a warning
    except ValueError:
        print('WARN: unable to extract supported IPython version from %s, defaulting to 7.0'%(IPython.__version__))
        ret=700
    _ipython_version=ret
    if fakedArgv: sys.argv=[]
    return ret


