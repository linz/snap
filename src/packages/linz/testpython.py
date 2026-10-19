#!/usr/bin/python
"""Prints the version of the Python that runs it, then the modules named on the command line that it cannot import.

findpython.pl runs it with each candidate Python, and uses the output to choose the Python for the LINZ snap plugin.
"""

from __future__ import print_function

import sys
import re

version = re.sub(r"\s.*", "", sys.version)

modules = sys.argv[1:]
missing = []
for m in modules:
    try:
        __import__(m)
    except ImportError:
        missing.append(m)

print(version, *missing)
