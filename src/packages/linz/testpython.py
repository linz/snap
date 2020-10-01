#!/usr/bin/python
from __future__ import print_function

import sys
import re

version = re.sub(r"\s.*", "", sys.version)

modules = sys.argv[1:]
missing = []
for m in modules:
    try:
        __import__(m)
    except:
        missing.append(m)

print(version, *missing)
