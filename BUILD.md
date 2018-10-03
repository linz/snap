BUILD INSTRUCTIONS
==================

Old instructions .. these require updating.  

Note that it is intended to implement cmake for these build tasks.  
Also boost and wxwidgets components need well defined installation processes.

Note that although source files include both .c and .cpp extensions they 
are all compiled as C++, which provides some better error checking etc.
Note also that the code currently generates a scary number of compiler 
warnings!

Build instructions Windows
==========================

In order to build SNAP the following tools must be installed on the build computer:

1) Microsoft Visual Studio 2014

2) perl (this has been built with the Activestate perl distribution, however other distributions should work)

3) hhc - the Microsoft HTML help compiler - assumed to be in a directory in the PATH variable.

4) boost libraries installed into a /boost subdirectory

5) wxWidgets built in the /wxwidgets directory (see the README.md file in that directory)


Build the snap programs
* Open ms/projects/snapwin.sln
* Set the configuration to "Debug" or "Release"
* Build the solution.

To build the installation (.msi) file
* Set the configuration to "Release"
* Build the snap_install project (note: this is not built by default)
The .msi file will be created in the ms/install/Release directory

Build instructions for Linux
============================

These instructions are not complete.  In as much as they are, they have 
been tested against a Ubunutu 18.04 amd64 platform.  They may require adapting for other 
distributions.

Install prerequisites (the last three are required to build a debian package 
for installation):

```
apt-get install -y \
    g++ \
    libboost-all-dev \
    libwxgtk3.0-dev \
    perl \
    debhelper \
    dpkg-dev \
    devscripts
```

The snap software is built using the makefile in the linux directory

```
cd linux
make
make test
```

(Note: for the release version running make test rebuilds some components as the
compilation date is updated by the build).

The software is built in the linux/release/install directory.

To build a debian package

```
make package
```

The package will be created in the root directory and can be installed using 
dpkg -i linz-snap-version.deb.  Note that this installs snap into the 
default path /usr/bin as runsnap.  This is to avoid conflict with the system snap
command.  The snap components are installed into /usr/share/linz/snap.  To use
the snap command itself you can install this directory into the path, for 
example in the .bashrc file 

```
export PATH=:/usr/share/linz/snap:${PATH}:/home/ccrook/bin
```




