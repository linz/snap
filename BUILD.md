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


Build instructions:

1) Build the wxWidgets library. 
   Open wxWidgets-2.8.4/build/msw/wx.sln
   Set the configuration to "Debug", and build the solution
   Set the configuration to "Release", and build the solution

2) Build the snap programs
   Open ms/projects/snapwin.sln
   Set the configuration to "Debug" or "Release"
   Build the solution.

   To build the installation (.msi) file
   Set the configuration to "Release"
   Build the snap_install project (note: this is not built by default)
   The .msi file will be created in the ms/install/Release directory

Build instructions for Linux
============================

These instructions are not complete.  In as much as they are, they have 
been tested against a Ubunutu 14.04 amd64 platform.  
They may require adapting for other operating systems.  

The prerequisites for building include at least g++ and some components of boost (regular expressions).

The build steps include:

1) Build wxWidgets 2.8 compoents. cd to wxwidgets-2.8, ./run_configure.sh, and then make

2) Build snap components (cd unix, make)

3) Copy unix/release/install to a suitable system directory, or add this directory
   to the PATH environment variable.


