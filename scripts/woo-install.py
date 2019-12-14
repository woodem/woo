#!/usr/bin/python3
import platform, os, sys, argparse, multiprocessing, os.path

if sys.version_info.major!=3: raise RuntimeError('This script should be run with python 3.x.')
if platform.system()!='Linux': raise RuntimeError('This script only runs under Linux.')
root=(os.getuid()==0)

dist,linver,codename=platform.linux_distribution()
if dist=='Ubuntu':
    # linver='16.04'
    if linver=='16.04': pass
    elif linver=='18.04': pass
    else: raise RuntimeError('Ubuntu version %s not supported by this script; see https://woodem.org/user/installation.html for other installation methods.'%linver)
else:
    raise RuntimeError('Linux distribution %s not supported by this script; see https://woodem.org/user/installation.html for other installation methods.'%dist)

print('Installing Woo on %s %s%s.'%(dist,linver,' (as root)' if root else ''))

parser=argparse.ArgumentParser(description='Script for automated installation of Woo on Linux, via compilation from latest source.')
parser.add_argument('-x','--headless',help='Compile headless version of woo (no OpenGL/Qt5).',action='store_true')
parser.add_argument('-j','--jobs',help='Number of threads for concurrent compilation.',type=int,default=multiprocessing.cpu_count())
parser.add_argument('--key',help='Key for extra modules (might be repeated).',action='append')
parser.add_argument('--ccache',help='Enable ccache for compilation.',default=False,action='store_true')
# parser.add_argument('--prefix',help='Prefix where to install. Its owner will be changed to the current user.',default='/usr/local')
parser.add_argument('--src',help='Directory where to put woo sources.',default=os.path.expanduser('~/woo'))
parser.add_argument('--git',help='Upstream git repository.',default='https://github.com/woodem/woo.git')
parser.add_argument('--build-prefix',help='Prefix for build files',default=os.path.expanduser('~/woo-build'))
parser.add_argument('--clean',help='Clean build files and caches after running; on Debian/Ubuntu also cleans APT cache. If given twice, all source files are also removed.',action='count')
parser.add_argument('--apt-no-update',help='Do not call apt update (for debugging)',default=False,action='store_true',dest='aptNoUpdate')
args=parser.parse_args()

if args.ccache and args.clean: print('WARN: --ccache and --clean do not make much sense together.')

venv=bool(os.getenv('VIRTUAL_ENV'))

import pwd, os

def call(cmd,failOk=False,sudo=False,cwd=None):
    import subprocess
    if sudo and not root: cmd=['sudo']+cmd
    print(' '.join(cmd))
    ret=subprocess.call(cmd,cwd=cwd)
    if not failOk and ret!=0: raise RuntimeError('Error calling: '+' '.join(cmd))

def gitprep(url,src,depth=-1):
    if os.path.exists(src):
        if not os.path.exists(src+'/.git'): raise RuntimeError('Source directory %s exists, but is not a git repository.'%src)
        call(['git','-C',src,'pull'])
    else:
        call(['git','clone']+([] if depth<1 else ['--depth',str(depth)])+[url,src])


if dist in ('Ubuntu','Debian'):
    if not args.aptNoUpdate: call(['apt','update'],sudo=True)
    call(['apt','install','--yes','eatmydata'],sudo=True)
    if dist=='Ubuntu':
        if linver=='18.04':
            aptCore='git cmake ninja-build python3-all python3-all-dev debhelper libboost-all-dev libvtk6-dev libgts-dev libeigen3-dev libhdf5-serial-dev mencoder ffmpeg python3-prettytable'.split()
            aptUI='python3-pyqt5 qtbase5-dev qtbase5-dev-tools pyqt5-dev-tools qt5-qmake qtchooser libgle3-dev libqglviewer-dev-qt5 libqt5opengl5-dev python3-pyqt5 python3-pyqt5.qtsvg freeglut3-dev python3-xlib'.split()
            pipCore='xlwt colour-runner git+https://github.com/eudoxos/minieigen11'.split()
            pipUI=[]
            if venv:
                pipCore+='numpy matplotlib future xlrd xlsxwriter colorama genshi psutil pillow h5py lockfile ipython prettytable '.split()
                pipUI+='vext.pyqt5 XLib'.split()
                aptUI+=['python3-pyqt5']
            else:
                aptCore+='python3-setuptools python3-pip python3-future python3-distutils python3-xlrd python3-xlsxwriter python3-numpy python3-matplotlib python3-colorama python3-genshi python3-psutil python3-pil python3-h5py python3-lockfile python3-future ipython3'.split()
                aptUI+='python3-pyqt5 python3-pyqt5.qtsvg python3-xlib'.split()
        else: raise RuntimeError('Unsupported Ubuntu version %s.'%linver)
        if args.ccache: aptCore+=['ccache']
    else: raise RuntimeError('Unsupported distribution %s'%dist)
    call(['eatmydata','apt','install','--yes']+aptCore+([] if args.headless else aptUI),sudo=True)

# install what is not packaged -- distribution-agnostic
call(['pip3','install','--upgrade']+pipCore+([] if args.headless else pipUI),sudo=True if not venv else False)

#if not root:
#    user=pwd.getpwuid(os.getuid()).pw_name
#    call(['chown','-R',user+':',args.prefix],sudo=True)

gitprep(args.git,args.src,depth=1)
for key in (args.key if args.key else []):
    gitprep('https://woodem.eu/private/%s/git'%key,args.src+'/wooExtra/'+key)

# compile
if args.ccache: call(['ccache','-M50G','-F10000'])
features=['gts','openmp','vtk','hdf5']+([] if args.headless else ['qt5'])

os.makedirs(args.build_prefix,exist_ok=True)
call(['cmake','-GNinja','-DWOO_PYBIND11=ON','-DWOO_FLAVOR=','-DWOO_CCACHE='+('ON' if args.ccache else 'OFF'),'-DWOO_BUILD_JOBS=%d'%args.jobs,'-DPYTHON_EXECUTABLE='+sys.executable]+['-DWOO_%s=ON'%f.upper() for f in features]+[args.src],cwd=args.build_prefix)
call(['ninja','-j%d'%args.jobs,'install'],cwd=args.build_prefix)
# call(['scons','-C',args.src,'flavor=','features='+','.join(features),'jobs=%d'%args.jobs,'buildPrefix='+args.build_prefix,'CPPPATH='+cpppath,'CXX='+('ccache g++' if args.ccache else 'g++'),'brief=1','debug=0','PYTHON='+sys.executable])
call(['woo','-j%d'%args.jobs,'--test'],failOk=True)

# testing only, not for system builds
if not root:
    call(['woo','-RR','-x'])

if args.clean:
    import shutil
    shutils.rmtree(args.build_prefix)
    if args.clean>1: shutils.rmtree(args.src)
    if dist in ('Ubuntu','Debian'): call(['apt','clean'],sudo=True) 
