BUILD INSTRUCTIONS
==================


Build instructions Windows
==========================

Note: Building 32bit windows versions is no longer maintained.  Only 64 bit Release and Debug configurations are still supported.

In order to build SNAP the following tools must be installed on the build computer:

1) Microsoft Visual Studio 2014

2) perl (this has been built with the Activestate perl distribution, however other distributions should work)

3) hhc - the Microsoft HTML help compiler - assumed to be in a directory in the PATH variable.  This is no longer maintained by MicroSoft.  Looking at alternatives!

4) boost libraries installed into a /boost subdirectory.  The boost libraries are downloaded from https://sourceforge.net/projects/boost/.  Currently installed using the prebuilt binaries and installing to snap directory, which creates boost subdirectory (but also overwrites README.md).  The boost version may need updating in ms/projects/snapwin.props.  

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

Tested against Ubuntu 22.04 (Jammy) and 24.04 (Noble) amd64.

Install prerequisites (the last three are required to build a debian package
for installation):

```
apt-get install -y \
    cmake \
    g++ \
    libboost-all-dev \
    libwxgtk3.2-dev \
    perl \
    python3 \
    debhelper \
    dpkg-dev \
    devscripts
```

The snap software is built using `build.py` in the root of the repository:

```
python3 build.py
python3 build.py release test
```

Build types are `release` (default), `debug`, and `profile`. The release build
is placed in `build-release/`. The GUI targets (snap_manager, snapadjust,
snapplot) are built by default; pass `--no-gui` to skip them. See
`python3 build.py --help` for all options.

To build a debian package (all changes must be committed first):

```
python3 build.py release package
```

The package will be created in the parent directory and can be installed using
`dpkg -i linz-snap-<version>.deb`. The snap components are installed into
`/usr/share/linz/snap`. To use the snap command itself you can add this
directory to the path, for example in the `.bashrc` file:

```
export PATH=/usr/share/linz/snap:${PATH}
```




