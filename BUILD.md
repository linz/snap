BUILD INSTRUCTIONS
==================

Old instructions .. these require updating.  

Note that it is intended to implement cmake for these build tasks.

Build instructions Windows
==========================

In order to build SNAP the following tools must be installed on the build computer:

1) Microsoft Visual Studio 2014

2) ActiveState perl

3) hhc - HTML help compiler - assumed to be in a directory in the PATH variable.


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

These instructions have been tested against a Ubunutu 14.04 amd64 platform.  
They may require adapting for other operating systems.

TBC
