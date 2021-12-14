***************
Installing Woo
***************

Linux
=====

Linux is the platform of choice for both developing and using Woo. Installation can be done in several ways, trading fexibility for straightforwardness.

Package installation
---------------------

Packages for Ubuntu are no longer maintained. Use compilation from source as described below.

Compilation from source
-----------------------
Source code is hosted at `GitHub <https://github.com/woodem/woo>`__ and can be otained with the `Git revision control system <http://git-scm.com/>`__ by saying ``git clone https://github.com/woodem/woo.git``.

Compilation on Ubuntu/Debian can use the  `woo-install.py <https://github.com/woodem/woo/blob/master/scripts/woo-install.py>`__ script which will also handle dependency installation. Other distributions should look into that file for necessary dependencies. The build system itself uses CMake and installs Woo as a compiled Python extension module (python virtual environments are supported).

CMake takes several options (see at the beginning of :woosrc:`CMakeLists.txt`) which will selectively turn on/off various features. The important options might be:

* `WOO_QT5`: enable or disable user interface; Woo can always run without GUI, though disabling it at build-time greatly reduces build dependencies (in particular Qt and OpenGL) [`ON` by default]
* `PYTHON_EXECUTABLE`: set for virtual environments [`/usr/bin/python3` by default]
* `WOO_FLAVOR`: suffix compiled woo module so that several can be installed and used in parallel [nothing by default].
* `WOO_DEBUG`: include debugging information in the build [`OFF` by default]

Windows
=======

`Windows Subsystem for Linux <https://docs.microsoft.com/en-us/windows/wsl/about>`__ makes it possible to run Ubuntu (and other distributions) inside Windows; `GUI apps <https://docs.microsoft.com/en-us/windows/wsl/tutorials/gui-apps>`__ can be also run (under recent builds of Windows). Thus for Windows installation, follow instructions for Ubuntu above.

