# this module is populated at initialization from the c++ part of PythonUI
"""Runtime variables, populated at woo startup."""
# default value
import wooMain
import woo.config

def _detectDisplay():
    import warnings,sys,os,platform
    import woo.config
    warnings.filterwarnings('ignore',category=DeprecationWarning,module='Xlib')

    if ('qt5' not in woo.config.features): return False,"compiled without GUI support (qt5 feature)."
    if wooMain.options.fakeDisplay: return False,'started with --fake-display'
    if wooMain.options.forceNoGui: return False,'started with the -n/--headless switch'
    if sys.platform=='win32': return True,'display always available under win32'
    if 'DISPLAY' not in os.environ: return False,'$DISPLAY env var is undefined'
    DISPLAY=os.environ['DISPLAY']
    if len(os.environ['DISPLAY'])==0: return False,'$DISPLAY env var is empty.'
    try: import Xlib.display
    except ImportError: raise RuntimeError("The python Xlib module could not be imported.")
    try:
        # for WSL, UNIX-domain socket will not work, thus hostname must be prepended to force IP connection
        # https://github.com/python-xlib/python-xlib/issues/99
        # this is not necessary for Qt below (it has that logic built-in already), only for testing the display via Xlib
        if 'Microsoft' in platform.release(): Xlib.display._BaseDisplay(('localhost:' if DISPLAY.startswith(':') else '')+DISPLAY)
        else: Xlib.display._BaseDisplay() # just use $DISPLAY
    except:
        # usually Xlib.error.DisplayError, but there can be Xlib.error.XauthError etc as well
        # let's just pretend any exception means the display would not work
        # warning with WSL
        if 'Microsoft' in platform.release() and platform.system()=='Linux' and os.environ['DISPLAY'].startswith(':'):
            print("WARNING: connection to $DISPLAY failed; since you are running under Microsoft Windows in WSL (probably), you need to export $DISPLAY with localhost: prefix. Currently, $DISPLAY is '%s', should be therefore 'localhost%s'."%(os.environ['DISPLAY'],os.environ['DISPLAY']))
        return False,'Connection to $DISPLAY failed.'
    return True,None


#
# if wooMain.options.fakeDisplay: hasDisplay=False # we would crash really
#
hasDisplay,hasDisplayError=_detectDisplay()


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


