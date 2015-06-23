
import sys
import json
from collections import namedtuple
import numpy as np

def load_obseqn( listfile, iteration=1 ):
    oejson=[]
    with open(listfile) as lst:
        started=False
        for l in lst:
            if l.startswith("BEGIN_JSON obs_equation_"+str(iteration)):
                started=True
            elif not started:
                continue
            elif l.startswith("END_JSON"):
                break
            else:
                oejson.append(l)

    if( len(oejson) == 0 ):
        print "Obs equations not dumped in listing file\n";
        sys.exit()

    oejson="".join(oejson)

    oe=json.loads("".join(oejson))
    return oe


if __name__ == "__main__":

    oe=load_obseqn(sys.argv[1])

    #print oe
    # Now try building and solving the normal equations.

    nprm=oe["nparam"]
    N=np.zeros((nprm,nprm))
    b=np.zeros((nprm,1))


    for ie,eqn in enumerate(oe["obs_equations"]):
        print "Summing obs eqns",ie
        nobs=eqn["nobs"]
        A=np.zeros((nprm,nobs))
        y=np.zeros((nobs,1))
        for iobs,obs in enumerate(eqn["obs"]):
            if not obs["useobs"]:
                raise RuntimeError("Cannot handle rejected observations")
            if "schreiber" in obs:
                raise RuntimeError("Schreiber eqns not handled")
            y[iobs,0]=obs["value"]
            for iprm,value in zip( obs["columns"], obs["values"] ):
                A[iprm-1,iobs]=value
        if "cvrdiag" in eqn:
            cvr=np.diagonal(eqn["cvrdiag"])
        else:
            cvr=np.array(eqn["cvr"])
        print "Wgt eigenvals",np.linalg.eigvals(cvr);
        wgt=np.linalg.inv(cvr)
        Aw=np.dot(A,wgt)
        b += np.dot(Aw,y)
        N += np.dot(Aw,A.T)

    print "Eigenvalues of N: ",np.linalg.eigvals(N)
    print "N:\n",N
    print "b:\n",b
    print "Inverting N"
    N = np.linalg.inv(N)
    print "Calculating x"
    x=np.dot(N,b)

    for p,xv in zip(oe["parameters"],x):
        print p,xv




        
