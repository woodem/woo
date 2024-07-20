# The purpose of this file is twofold: 
# 1. it tells python that woo (this directory) is package of python modules
#    see http://http://www.python.org/doc/2.1.3/tut/node8.html#SECTION008400000000000000000
#
# 2. import the runtime namespace (will be populated from within c++)
#

"""Common initialization core for woo.

This file is executed when anything is imported from woo for the first time.
It loads woo plugins and injects c++ class constructors to the __builtins__
(that might change in the future, though) namespace, making them available
everywhere.
"""

from wooMain import options as wooOptions
import warnings,traceback
import sys,os,os.path,re,string


# this would be true even when running under WSL (Linux)
# so just disable Windows completely; never worked well, anyway
WIN=False # sys.platform=='win32'
PY3K=(sys.version_info[0]==3)

if WIN:
    class WooOsEnviron(object):
        '''Class setting env vars via both CRT and win32 API, so that values can be read back
        with getenv. This is needed for proper setup of OpenMP (which read OMP_NUM_THREADS).'''
        def __setitem__(self,name,val):
            import ctypes
            # in windows set, value in CRT in addition to the one manipulated via win32 api
            # http://msmvps.com/blogs/senthil/archive/2009/10/413/when-what-you-set-is-not-what-you-get-setenvironmentvariable-and-getenv.aspx
            ## use python's runtime
            ##ctypes.cdll[ctypes.util.find_msvcrt()]._putenv("%s=%s"%(name,value))
            # call MSVCRT (unversioned) as well
            ctypes.cdll.msvcrt._putenv("%s=%s"%(name,val))
            os.environ[name]=val
        def __getitem__(self,name): return os.environ[name]
    wooOsEnviron=WooOsEnviron()
    # this was set in wooMain (with -D, -vv etc), set again so that c++ sees it
    if 'WOO_DEBUG' in os.environ: wooOsEnviron['WOO_DEBUG']=os.environ['WOO_DEBUG']
else:
    wooOsEnviron=os.environ
    

# this is for GTS imports, must be set before compiled modules are imported
wooOsEnviron['LC_NUMERIC']='C'

# we cannot check for the 'openmp' feature yet, since woo.config is a compiled module
# we set the variable as normally, but will warn below, once the compiled module is imported
if wooOptions.ompCores:
    cc=wooOptions.ompCores
    if wooOptions.ompThreads!=len(cc) and wooOptions.ompThreads>0:
        print('wooOptions.ompThreads =',str(wooOptions.ompThreads))
        warnings.warn('ompThreads==%d ignored, using %d since ompCores are specified.'%(wooOptions.ompThreads,len(cc)))
    wooOptions.ompThreads=len(cc)
    wooOsEnviron['GOMP_CPU_AFFINITY']=' '.join([str(cc[0])]+[str(c) for c in cc])
    wooOsEnviron['OMP_NUM_THREADS']=str(len(cc))
elif wooOptions.ompThreads:
    wooOsEnviron['OMP_NUM_THREADS']=str(wooOptions.ompThreads)
elif 'OMP_NUM_THREADS' not in os.environ:
    import multiprocessing
    # only set for the when running for real, otherwise the subprocess will report the env var which is confusing
    if not wooOptions.rebuilding: wooOsEnviron['OMP_NUM_THREADS']=str(multiprocessing.cpu_count())
elif 'OMP_NUM_THREADS' in os.environ:
    print(f'Honoring environment variable OMP_NUM_THREADS={os.environ["OMP_NUM_THREADS"]}.')

import sysconfig
soSuffix=sysconfig.get_config_vars()['SO']

#
# QUIRKS
#
# not in Windows, and not when running without X (the check is not very reliable
if not WIN and (wooOptions.quirks & wooOptions.quirkIntel) and 'DISPLAY' in os.environ:
    import os,subprocess
    try:
        vgas=subprocess.check_output("LC_ALL=C lspci | grep VGA",shell=True,stderr=subprocess.STDOUT,universal_newlines=True).split('\n')
        if len(vgas)==1 and 'Intel' in vgas[0]:
            # popen does not raise exception if it fails
            try:
                glx=subprocess.check_output("LC_ALL=C glxinfo | grep 'OpenGL version string:'",shell=True,stderr=subprocess.STDOUT,universal_newlines=True).split('\n')
                # this should cover broken drivers down to Ubuntu 12.04 which shipped Mesa 8.0
                if len(glx)==1 and re.match(r'.* Mesa (9\.[01]|8\.0)\..*',glx[0]):
                    print('Intel GPU + Mesa < 9.2 detected, setting LIBGL_ALWAYS_SOFTWARE=1\n\t(use --quirks=0 to disable)')
                    os.environ['LIBGL_ALWAYS_SOFTWARE']='1'
            except subprocess.CalledProcessError: pass # failed glxinfo call, such as when not installed
    except subprocess.CalledProcessError: pass # failed lspci call...?!


# enable warnings which are normally invisible, such as DeprecationWarning
warnings.simplefilter('default')
# disable warning for unclosed files/sockets
warnings.simplefilter('ignore',ResourceWarning)
warnings.filterwarnings(action='ignore',category=DeprecationWarning,module='unittest2')
warnings.filterwarnings(action='ignore',category=DeprecationWarning,module=r'matplotlib\..*')


import importlib,importlib.machinery,importlib.util
def imp_load_dynamic(name,path):
    loader=importlib.machinery.ExtensionFileLoader(name,path)
    spec=importlib.machinery.ModuleSpec(name=name,loader=loader,origin=path)
    return importlib.util.module_from_spec(spec)


# c++ initialization code
cxxInternalName='_cxxInternal'
if wooOptions.flavor: cxxInternalName+='_'+re.sub('[^a-zA-Z0-9_]','_',wooOptions.flavor)
# if wooOptions.debug: cxxInternalName+='_debug'
try: _cxxInternal=importlib.import_module('woo.'+cxxInternalName)
except ImportError:
    print(f'Error importing woo.{cxxInternalName} (--flavor={wooOptions.flavor if wooOptions.flavor else " "}).')
    traceback.print_exc()
    import glob
    sos=glob.glob(re.sub('__init__.py$','',__file__)+'/_cxxInternal_*'+soSuffix)
    flavs=[re.sub('(^.*/_cxxInternal_)(.*)(\\'+soSuffix+'$)',r'\2',so) for so in sos]
    if sos:
        maxFlav=max([len(flav) for flav in flavs])
        print('\nAvailable Woo flavors are:\n--------------------------')
        for so,flav in zip(sos,flavs):
            print('\t{0: <{1}}\t{2}'.format(flav,maxFlav,so))
        print()
    raise
sys.modules['woo._cxxInternal']=_cxxInternal

cxxInternalFile=_cxxInternal.__file__

from . import core
master=core.Master.instance
from . import apiversion

#
# create compiled python modules
#
if sys.version_info<(3,4): 
    print('WARNING: in Python 3.x, importing only works in Python >= 3.4 properly. Your version %s will most likely break right here.'%(sys.version))
# will only work when http://bugs.python.org/issue16421 is fixed (python 3.4??)
allSubmodules=set()
# print(80*'#'+'\n'+str(master.compiledPyModules))
for mod in master.compiledPyModules:
    if 'WOO_DEBUG' in os.environ: print('Loading compiled module',mod,'from',cxxInternalFile)
    # this inserts the module to sys.modules automatically
    m=imp_load_dynamic(mod,cxxInternalFile)
    # now put the module where it belongs
    mm=mod.split('.')
    if mm[0]!='woo': print('ERROR: non-woo module %s imported from the shared lib? Expect troubles.'%mod)
    elif len(mm)==2:
        allSubmodules.add(mm[1])
        globals()[mm[1]]=m
    else: setattr(eval('.'.join(mm[1:-1])),mm[-1],mm)


# from . import config # does not work in PY3K??
# import woo.config
config=sys.modules['woo.config']
if 'gts' in config.features:
    if 'gts' in sys.modules and sys.modules['gts'].__name__!='woo.gts': raise RuntimeError("Woo was compiled with woo.gts, buts gts modules is already imported from elsewhere (%s, module %s)? Since two implementations may clash, this is not allowed."%(sys.modules['gts'].__file__,sys.modules['gts'].__name__))
    from . import gts
    # so that it does not get imported twice
    sys.modules['gts']=gts

if False:
    #print('WARN: hijacking minieigen module, points to woo.eigen.')
    #if 'minieigen' in sys.modules: del sys.modules['minieigen'] # 'unimport'
    #sys.modules['minieigen']=sys.modules['woo.eigen']
    #assert(woo.eigen.Vector3.__class__.__name__=='pybind11_type')
    assert(id(sys.modules['minieigen'])==id(sys.modules['_wooEigen11']))
    import _wooEigen11
    assert(_wooEigen11.Vector3.__class__.__name__=='pybind11_type')
    import numpy as np
    from typing import NewType
    class NpArrayWrap(np.ndarray):
        def __new__(cls,*args): return np.array(args)
    for t in ('Vector2','Vector2i','Vector3i','Vector6','Vector6i','VectorX','Matrix6','MatrixX'):
        setattr(_wooEigen11,t,NpArrayWrap) # NewType(t,np.array))
    import minieigen
    assert(minieigen.Vector3.__class__.__name__=='pybind11_type')
    from minieigen import *
    print(minieigen.Vector3)

if 'pybind11' in config.features:
    assert(id(sys.modules['minieigen'])==id(sys.modules['_wooEigen11']))
    import _wooEigen11
    assert(_wooEigen11.Vector3.__class__.__name__=='pybind11_type')
    import minieigen
    assert(minieigen.Vector3.__class__.__name__=='pybind11_type')
    from minieigen import *
else:
    import minieigen


from . import runtime

##
## temps cleanup at exit
##
import atexit, shutil, threading, glob
## clean old temps in bg thread
def cleanTemps(tmpDir,what='old'):
    assert what in ('old','my')
    try: import psutil
    except ImportError:
        sys.stderr.write('** Not cleaning old temps, since the psutil module is missing.\n')
        return
    # sys.stderr.write(f'Pattern is {tmpDir}/~woo-tmp_p*_*\n')
    ddel=[]
    for d in glob.glob(tmpDir+'/~woo-tmp_p*_*'):
        # sys.stderr.write(f'## {d}\n')
        m=re.match('~woo-tmp_p([0-9]+)_.*',os.path.basename(d))
        if not m:
            sys.stderr.write('** Unparseable temporary file name: {%d}\n')
            continue
        # sys.stderr.write(str(m.group(1))+'\n')
        pid=int(m.group(1))
        if what=='old' and psutil.pid_exists(pid): continue
        if what=='my' and pid!=os.getpid(): continue
        if os.path.isdir(d): shutil.rmtree(d)
        else: os.remove(d)
        ddel.append(d)
    if ddel and 'WOO_DEBUG' in wooOsEnviron: sys.stderr.write(f'Purged {what} temps: {" ".join(ddel)}\n')
threading.Thread(target=cleanTemps,args=(master.tmpFileDir,'old')).start()
# this would not work under windows, probably (?)
if not WIN:
    atexit.register(cleanTemps,master.tmpFileDir,what='my')


##
## Warn if OpenMP threads are not what is probably expected by the user
##
if not wooOptions.rebuilding:
    if wooOptions.ompThreads>1 or wooOptions.ompCores:
        if 'openmp' not in config.features:
            warnings.warn('--threads and --cores ignored, since compiled without OpenMP.')
        elif master.numThreads!=wooOptions.ompThreads:
            warnings.warn('--threads/--cores did not set number of OpenMP threads correctly (requested %d, current %d). Was OpenMP initialized in this process already?'%(wooOptions.ompThreads,master.numThreads))
    elif master.numThreads>1:
        if 'OMP_NUM_THREADS' in os.environ:
            if master.numThreads!=int(os.environ['OMP_NUM_THREADS']): warnings.warn('OMP_NUM_THREADS==%s, but woo.master.numThreads==%d'%(os.environ['OMP_NUM_THREADS'],master.numThreads))
        else:
            warnings.warn('OpenMP is using %d cores without --threads/--cores being used - the default should be 1'%master.numThreads)

if wooOptions.clDev:
    if 'opencl' in config.features:
        if wooOptions.clDev:
            try:
                clDev=[int(a) for a in wooOptions.clDev.split(',')]
                if len(clDev)==1: clDev.append(-1) # default device
                if not len(clDev) in (1,2): raise ValueError()
            except (IndexError, ValueError, AssertionError):
                raise ValueError('Invalid --cl-dev specification %s, should an integer (platform), or a comma-separated couple (platform,device) of integers'%opts.clDev)
            master.defaultClDev=clDev
    else: warnings.warn("--cl-dev ignored, since compiled without OpenCL.")


__all__=['master']+list(allSubmodules)

### IMPORTANT: deal with finalization issues (crashes at shutdown)
from . import system
system.setExitHandlers()


# fake miniEigen being in woo itself
from minieigen import *

# monkey-patches
from . import _monkey
from . import _units
unit=_units.unit # allow woo.unit['mm']
# hint fo pyinstaller to freeze this module
from . import pyderived
from . import apiversion


if 1:
    # recursive import of everything under wooExtra
    try:
        # don't import at all if rebuilding (rebuild might fail)
        if wooOptions.rebuilding: raise ImportError
        import wooExtra
        import pkgutil, zipimport
        extrasLoaded=[]
        for importer, modname, ispkg in pkgutil.iter_modules(wooExtra.__path__,'wooExtra.'):
            try:
                try:
                    m=__import__(modname,fromlist='wooExtra')
                except:
                    print(50*'#')
                    import traceback
                    traceback.print_exc()
                    print(50*'#')
                    raise
                extrasLoaded.append(modname)
                if hasattr(sys,'frozen') and not hasattr(m,'__loader__') and len(m.__path__)==1:
                    zip=m.__path__[0].split('/wooExtra/')[0].split('\\wooExtra\\')[0]
                    if not (zip.endswith('.zip') or zip.endswith('.egg')):
                        print('wooExtra.%s: not a .zip or .egg, no __loader__ set (%s)'%(modname,zip))
                    else:
                        print('wooExtra.%s: setting __loader__ and __file__'%modname)
                        m.__loader__=zipimport.zipimporter(zip)
                        m.__file__=os.path.join(m.__path__[0],os.path.basename(m.__file__))
            except ImportError:
                sys.stderr.write('ERROR importing %s:'%modname)
                raise
        # disable informative message if plain import into python script
        # if sys.argv[0].split('/')[-1].startswith('woo'): sys.stderr.write('wooExtra modules loaded: %s.\n'%(', '.join(extrasLoaded)))
    except ImportError:
        # no wooExtra packages are installed
        pass

