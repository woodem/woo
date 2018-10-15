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
        if IPython.__version__.startswith('0.'): ret=int(IPython.__version__.split('.',2)[1]) ## convert '0.10' to 10, '0.11.alpha1.bzr.r1223' to 11
        elif IPython.__version__.startswith('1.0'): ret=100
        elif IPython.__version__.startswith('1.1'): ret=110
        elif IPython.__version__.startswith('1.2'): ret=120
        elif IPython.__version__.startswith('2.0'): ret=200
        elif IPython.__version__.startswith('2.1'): ret=210
        elif IPython.__version__.startswith('2.2'): ret=220
        elif IPython.__version__.startswith('2.3'): ret=230
        elif IPython.__version__.startswith('2.4'): ret=240
        elif IPython.__version__.startswith('3.'):  ret=300
        elif IPython.__version__.startswith('4.'):  ret=400
        elif IPython.__version__.startswith('5.'):  ret=500
        elif IPython.__version__.startswith('6.'):  ret=600
        elif IPython.__version__.startswith('7.'):  ret=700
        elif IPython.__version__.startswith('8.'):  ret=800
        else: raise ValueError() # nothing detected, issue a warning
    except ValueError:
        print('WARN: unable to extract IPython version from %s, defaulting to 5.0'%(IPython.__version__))
        ret=500
    # if ret in (500,600): raise RuntimeError('IPython 5.x and 6.x are not yet supported.')
    if ret<100: raise RuntimeError('IPython <= 1.0 is not supported anymore.')
    if ret not in (100,110,120,200,210,220,230,240,300,400,500,600,700,800): # versions that we are able to handle, round up or down correspondingly
        newipver=200
        print('WARN: unhandled IPython version %d.%d, assuming %d.%d instead.'%(ret%100,ret//100,newipver%100,newipver//100))
        ret=newipver
    _ipython_version=ret
    if fakedArgv: sys.argv=[]
    return ret


