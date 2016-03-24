#!/bin/bash
set -e -x
echo "*** This script sets up environment with Woo. Read the script before continuing. ***"
echo
echo "    1. Required packages will be installed"
echo "    2. /usr/local is chown'ed to the current user so that we can install there without sudo"
echo "    3. ~/woo is created/updated to the latest upstream source"
echo "    4. ~/woo/wooExtra is populated with extra modules, which are given as arguments to the script (if any)"
echo "    5. Woo is compiled and installed."
echo
echo "*** You will be asked for root password several times. That is normal. ***"

ISSUE=`cat /etc/issue`

case $ISSUE in
	*Xenial*|*16.04*)
		;;
	*)
		echo '*** YOUR UBUNTU INSTALLATION IS NOT RECOGNIZED, ONLY 16.04 AND NEWER ARE SUPPORTED***'
		exit 1
		;;
esac

# faster install
sudo apt install eatmydata
# install libs & headers
sudo eatmydata apt install --yes libboost-all-dev libqt4-dev-bin libqt4-dev qt4-dev-tools libgle3-dev libqglviewer-dev-qt4 libvtk6-dev libgts-dev libeigen3-dev freeglut3-dev git ccache scons libav-tools libhdf5-serial-dev paraview
# install python-related packages
sudo eatmydata apt install --yes python3-all-dev python3-setuptools python3-pip python3-xlrd python3-xlsxwriter python3-numpy python3-matplotlib python3-pyqt4 python3-xlib python3-genshi python3-psutil python3-pil python3-h5py python3-lockfile python3-minieigen python3-prettytable python3-colorama ipython3 pyqt4-dev-tools python3-future
# install python stuff that is not packaged in Ubuntu
sudo pip3 install --system xlwt-future colour-runner

# 
sudo chown -R $USER: /usr/local


if [ ! -d ~/woo/.git ]; then
	rm -rf ~/woo # in case it contains some garbage
	git clone --depth 1 https://github.com/woodem/woo.git ~/woo
else
	git -C ~/woo pull
fi

# grab extra modules
for KEY in "$@"; do
	DEST=~/woo/wooExtra/$KEY
	if [ ! -d $DEST/.git ]; then
		rm -rf $DEST
		git clone http://woodem.eu/private/$KEY/git $DEST
	else
		git -C $DEST pull
	fi
done

# compile
mkdir -p ~/woo-build
ccache -M50G -F10000 # adjust maxima for ccache
scons -C ~/woo flavor= features=gts,opengl,openmp,qt4,vtk,hdf5 jobs=4 buildPrefix=~/woo-build CPPPATH=`ls -d /usr/include/vtk-6.*`:/usr/include/eigen3:/usr/include/hdf5/serial CXX='ccache g++' brief=0 debug=0 PYTHON=`which python3`

## tests
# crash at exit perhaps, should be fixed
woo -j4 --test
## test update (won't recompile, anyway)
woo -RR -x
echo "*** Woo has been installed successfully. ***"
