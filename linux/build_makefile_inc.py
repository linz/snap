#!/usr/bin/python3
import os
import re
from os.path import dirname, join, abspath

base=join(dirname(dirname(abspath(__file__))),'src')

objdirs = []
modules = {}

for fbase, dirs, files in os.walk(base):
    fbase = fbase[len(base):]
    if '.svn' in dirs:
        dirs.remove('.svn')
    objects = None

    for f in files:
        match = re.match(r'(.*)\.c(pp)?$',f)
        if match:
            file = join(fbase,match.group(1)+'.o')
            module = file.split(os.path.sep)[1]
            if module not in modules:
                modules[module] = []
            objdir = os.path.dirname( file )
            if objdir not in objdirs:
                objdirs.append(objdir)
            modules[module].append(file)

if 'snapadjust' in modules:
    for f in modules.get('snap',[]):
        if not f.endswith('/main.o'):
            modules['snapadjust'].append(f)

print( '''
OBJDIRS = \\
    $(TYPE) \\
    $(INSTALL) \\
    $(OBJ) \\''')

for dir in sorted(objdirs):
    print( "    $(OBJ)"+dir+ " \\")

print('')

for m in sorted(modules.keys()):
    files = modules[m]
    m = m.upper()+'OBJS'
    print(m,' = \\')
    for file in files:
        print("    $(OBJ)"+file+ " \\")
    print('')
