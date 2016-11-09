SNAP survey network adjustment package
======================================

This is the source code for the Land Information New Zealand (LINZ) survey network
adjustment program SNAP.  The principal purpose of this package is network adjustment 
of land survey observations, such as GNSS vectors, distances and angles, to calculate
coordinates of survey marks and to investigate the quality of the observations.  

The package is focussed on adjustments in New Zealand.  To do this it takes into account 
the NZGD2000 datum and in particular the deformation model defining the ongoing 
tectonic deformation of the datum.  However most of the functionality applies equally well elsewhere.

The SNAP package includes the following main components:

* snap: the main adjustment program.  This is a command line program using text data files 
   and configuration files defining the job to run
* snapplot: a program for displaying the survey marks and observations used in the adjustment
   and for reviewing and analysing the statistical information.
* a number of utility programs for managing snap data files (including station coordinate files)
* snap_manager: a graphical user interface for running the various programs in the package
* html help files defining the file formats used, the configuration of the software, 
  and options for running the programs.
  
The documentation is viewable from the [source help files](http://htmlpreview.github.io/?https://raw.githubusercontent.com/linz/snap/master/src/help/help/index.html)

Developer notes
---------------

SNAP is now approximately 25 years old, being originally written to run on IBM XT compatible
computers.  The original code and data structures were constructed to optimise memory
usage, often at the expense of clarity.  Also much of the string handling is very traditional
C code manipulating character pointers.  The entire code base is due for refactoring to use 
more modern practices and tools, such as C++ classes, string handling, STL and so on.  Likewise 
the graphical user interface is currently built on WxWidgets version 2.8.  This requires significant
work to port to version 3.0, in particular because of the changes to support unicode.  Also 
at present there is not consistent good build system.  It is intended that cmake will be used
for building in the future.

It is not recommended that this code is used as a basis for building further tools or for 
building capability from.  The main intent in releasing as open source code is to provide 
for compiling the package to work in otherwise unsupported operating systems.  

Currently LINZ provides Microsoft Windows binaries for the package. The code base can also be 
compiled into linux components.  Currently this has been tested in Ubuntu 14.04. See [BUILD.md](BUILD.md)
for more information on building the software.

Disclaimer
----------

Land Information New Zealand does not offer any support for this software.
The software is provided "as is" and without warranty of any kind. 
In no event shall LINZ be liable for loss of any kind whatsoever with respect to
the download, installation and use of the software.
The software is designed to work with Microsoft Windows.
However, LINZ makes no warranty regarding the performance or non-performance
of the software on any particular system or system configuration.


Licence
-------

Except for the wxwidget-2.8 directory the contents of this repository are licensed 
under the MIT licence as in [LICENCE.md](LICENCE.md).  The wxwidgets-2.8 directory contains
the [https://www.wxwidgets.org](wxWidgets library).  The licence for this code is in the 
[/linz/snap/blob/master/wxwidgets-2.8/docs/licendoc.txt](wxwidgets-2.8/docs/licendoc.txt).

