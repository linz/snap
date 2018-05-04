from __future__ import print_function
import re
import csv
import os
import sys
import subprocess
from collections import namedtuple

description='''
Create a SNAP coordinate file from a PositioNZ station coordinate file list.

The PositioNZ station coordinates are created using calc_refstation_coordinates
script.  This script uses snapconv to convert the ITRF2008 coordinates to 
NZGD2000, to ensure the conversion is using the same version of the deformation
model as SNAP.
'''

if len(sys.argv) != 3:
    print("Need parameters: positionz_coordinate_csv snap_coordinate_file")

pnzfile=sys.argv[1]
crdfile=sys.argv[2]

expected_fields='code scm_version deformation_version calc_date itrf2008_X itrf2008_Y itrf2008_Z'.split()

Station=namedtuple('Station','code X Y Z'.split())
stations=[]
scm_versions=set()

with open(pnzfile, 'rb') as pnzf:
    cdr=csv.DictReader(pnzf)
    missing=[x for x in expected_fields if x not in cdr.fieldnames]
    if missing:
        raise RuntimeError('Missing fields '+' '.join(missing)+' in '+pnzfile)
    for l in cdr:
        code=l['code']
        scm_versions.add(l['scm_version'].split()[0])
        dfm_version=l['deformation_version']
        calc_date=l['calc_date']
        x=l['itrf2008_X']
        y=l['itrf2008_Y']
        z=l['itrf2008_Z']
        stations.append(Station(code,x,y,z))

if len(stations) < 1:
    raise RuntimeError('No stations found in {0}'.format(pnzfile))

print("Found {0} stations".format(len(stations)))
print("Calc date: {0}".format(calc_date))
print("SCM version(s): {0}".format(' '.join(sorted(scm_versions))))

crditrf=crdfile+'.itrf2008'
with open(crditrf,'w') as crdf:
    crdf.write("PositioNZ-PP coordinates at {0}\n".format(calc_date))
    crdf.write("ITRF2008_XYZ\n")
    crdf.write("options no_geoid\n\n")
    for s in stations:
        crdf.write("{0} {1} {2} {3} {0}\n".format(s.code,s.X,s.Y,s.Z))

print("ITRF2008 coordinates in {0}".format(crditrf))
subprocess.call(['snapconv','-y',calc_date,'-e','-d',crditrf,'NZGD2000',crdfile])
subprocess.call(['snapgeoid','-h','NZVD2016',crdfile,crdfile])


print("NZGD2000 coordinates in {0}".format(crdfile))
