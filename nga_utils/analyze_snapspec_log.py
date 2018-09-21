#!/usr/bin/python

import argparse
import sys
import re

parser=argparse.ArgumentParser(description='Analyze data in snapspec log')
parser.add_argument('snapspec_log_file',help='Input snapspec log file')
parser.add_argument('output_csv_file',help='Output csv file')
args=parser.parse_args()

logfile=args.snapspec_log_file
csvfile=args.output_csv_file

testre=[
    (r'\s+Station\s+(?P<stn1>\w+)\s+(?P<status>passes)\s+(?:vrt\s+)?rel\s+acc\s+from\s+abs\s+accuracy\s+\((?P<v1>[\d\.]+)\s+\<\s+(?P<v2>[\d\.]+)\)','RAFA'),
    (r'\s+Station\s+(?P<stn1>\w+)\s+(?P<status>fails)\s+(?:vrt\s+)?abs\s+accuracy\s+\((?P<v1>[\d\.]+)\s+\>\s+(?P<v2>[\d\.]+)\)','AA'),
    (r'\s+Station\s+(?P<stn1>\w+)\s+(?P<status>fails)\s+on\s+rel\s+(?:vrt\s+)?accuracy\s+to\s+passed\s+station\s+(?P<stn2>\w+)\s+\((?P<v1>[\d\.]+)\s+\>\s+(?P<v2>[\d\.]+)\)','RA'),
    (r'\s+Station\s+(?P<stn1>\w+)\s+(?P<status>passes)\s+rel\s+accuracy\s+tests','RA'),
    ]

skipre=[
    r'\s+Setting\s+order\s+of\s+node\s+(?P<stn>\w+)\s+to\s+(\w+)',
    ]

testre=[(re.compile(tre[0]),tre[1]) for tre in testre]
skipre=[re.compile(tre) for tre in skipre]

csvcols=['phase','order','stn1','stn2','test','status','v1','v2']


def parse_snapspec_tests( logf ):
    phase=''
    order=''
    nphase=0
    nbad=0
    phasere=re.compile(r'Running\s+test\s+for\s+order\s+(?P<order>\w+)')
    for l in logf:
        m=phasere.match(l)
        if m:
            nphase+=1
            phase=str(nphase)
            order=m.group('order')
            continue
        if nphase==0:
            continue
        matched=False
        for tre,test in testre:
            m=tre.match(l)
            if m:
                result=m.groupdict()
                result['status']=result['status'][0].upper()
                result['phase']=phase
                result['order']=order
                result['test']=test
                yield result
                matched=True
                break
        for tre in skipre:
            if tre.match(l):
                matched=True
                break
        if not matched:
            print l.rstrip()
            nbad+=1
            if nbad > 1000:
                break

with open(logfile) as logf, open(csvfile,'w') as csvf:
    csvf.write(','.join(csvcols))
    csvf.write('\n')
    for r in parse_snapspec_tests(logf):
        data=[r.get(c,'') for c in csvcols]
        csvf.write(','.join(data))
        csvf.write('\n')
