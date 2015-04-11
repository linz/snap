
import sys
import json
from collections import namedtuple
import numpy as np

def check_file_json( filename ):
    print "Checking JSON in",filename
    objson=[]
    with open(filename) as lst:
        started=False
        name=''
        for l in lst:
            if l.startswith("BEGIN_JSON"):
                started=True
                name=l[10:].strip()
            elif not started:
                continue
            elif l.startswith("END_JSON"):
                started=False
                try:
                    obj=json.loads("".join(objson))
                    print " ",name,"OK"
                except:
                    print "*** Failed to read JSON:",name,"\n",sys.exc_info()[1]
                objson=[]
            else:
                objson.append(l)

for filename in sys.argv[1:]:
    try:
        check_file_json( filename )
    except:
        print "Failed file",filename,": ",str(sys.exc_info()[1])





