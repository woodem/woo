#!/usr/bin/python3
import platform, os, sys, argparse, multiprocessing, os.path

if sys.version_info.major!=3: raise RuntimeError('This script should be run with python 3.x.')
if platform.system()!='Linux': raise RuntimeError('This script only runs under Linux.')

dist,linver,codename=platform.linux_distribution()
if dist=='Ubuntu':
    # linver='16.04'
    if linver=='16.04': pass
    else: raise RuntimeError('Ubuntu version %s not supported by this script; see https://woodem.org/user/installation.html for other installation methods.'%linver)
else:
    raise RuntimeError('Linux distribution %s not supported by this script; see https://woodem.org/user/installation.html for other installation methods.'%dist)

print('Installing Woo on %s %s.'%(dist,linver))

parser=argparse.ArgumentParser(description='Script for automated installation of Woo on Linux, via compilation from latest source.')
parser.add_argument('-x','--headless',help='Compile headless version of woo (no OpenGL/Qt5).',action='store_true')
parser.add_argument('-j','--jobs',help='Number of threads for concurrent compilation.',type=int,default=multiprocessing.cpu_count())
parser.add_argument('--key',help='Key for extra modules (might be repeated).',action='append')
parser.add_argument('--ccache',help='Enable ccache for compilation.',default=False,action='store_true')
parser.add_argument('--prefix',help='Prefix where to install. Its owner will be changed to the current user.',default='/usr/local')
parser.add_argument('--src',help='Directory where to put woo sources.',default=os.path.expanduser('~/woo'))
parser.add_argument('--git',help='Upstream git repository.',default='https://github.com/woodem/woo.git')
parser.add_argument('--build-prefix',help='Prefix for build files',default=os.path.expanduser('~/woo-build'))
# parser.add_argument('--no-eatmydata',desc='Avoid eatmydata wrapper for faster APT package installation',default=True,action='store_true')
args=parser.parse_args()

import pwd, os

def call(cmd,failOk=False):
    import subprocess
    print(' '.join(cmd))
    ret=subprocess.call(cmd)
    if not failOk and ret!=0: raise RuntimeError('Error calling: '+' '.join(cmd))

def gitprep(url,src,depth=-1):
    if os.path.exists(src):
        if not os.path.exists(src+'/.git'): raise RuntimeError('Source directory %s exists, but is not a git repository.'%src)
        call(['git','-C',src,'pull'])
    else:
        call(['git','clone']+([] if depth<1 else ['--depth',str(depth)])+[url,src])


if dist in ('Ubuntu','Debian'):
    call(['sudo','apt','update'])
    call(['sudo','apt','install','--yes','eatmydata'])
    if dist=='Ubuntu':
        if linver=='16.04':
            aptCore='libboost-all-dev libvtk6-dev libgts-dev libeigen3-dev git scons libav-tools libhdf5-serial-dev python3-all-dev python3-setuptools python3-pip python3-xlrd python3-xlsxwriter python3-numpy python3-matplotlib python3-genshi python3-psutil python3-pil python3-h5py python3-lockfile python3-minieigen python3-prettytable python3-colorama ipython3 python3-future'.split()
            if args.ccache: aptCore+=['ccache']
            aptUI='libqt4-dev-bin libqt4-dev qt4-dev-tools libgle3-dev libqglviewer-dev-qt4 paraview freeglut3-dev python3-pyqt4 python3-xlib pyqt4-dev-tools'.split()
            pipCore='xlwt-future colour-runner'.split()
            pipUI=[]
            qtFeature='qt4'
        else: raise RuntimeError('unsupported')
        import glob
    else: raise RuntimeError('unsupported')
    call(['sudo','eatmydata','apt','install','--yes']+aptCore+([] if args.headless else aptUI))
    # this must be done AFTER pkg installation so that VTK can be found
    cpppath='/usr/include/eigen3:/usr/include/hdf5/serial:'+glob.glob('/usr/include/vtk-6*')[0]

# install what is not packaged -- distribution-agnostic
call(['sudo','pip3','install','--upgrade','--system']+pipCore+([] if args.headless else pipUI))

user=pwd.getpwuid(os.getuid()).pw_name
call(['sudo','chown','-R',user+':',args.prefix])
    
gitprep(args.git,args.src,depth=1)
for key in (args.key if args.key else []):
    gitprep('https://woodem.eu/private/%s/git'%key,args.src+'/wooExtra/'+key)

    # compile
if args.ccache: call(['ccache','-M50G','-F10000'])
features=['gts','openmp','vtk','hdf5']+([] if args.headless else [qtFeature,'opengl'])
call(['scons','-C',args.src,'flavor=','features='+','.join(features),'jobs=%d'%args.jobs,'buildPrefix='+args.build_prefix,'CPPPATH='+cpppath,'CXX='+('ccache g++' if args.ccache else 'g++'),'brief=1','debug=0','PYTHON='+sys.executable])
call(['woo','-j%d'%args.jobs,'--test'],failOk=True)
call(['woo','-RR','-x'])
