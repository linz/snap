#!/usr/bin/env python3
"""
Generates derived snapspec test configurations.

Called by testall.pl before it globs for */test.config, so that any
generated suites exist in time to be picked up and run.

Currently generates one derived suite:

  snapspec_k/  — identical to snapspec/ but passes -k (KD-tree spatial
                 index) to snapspec.  Input files and reference output
                 (check/) are shared with snapspec/; only the out/
                 directory is written locally to snapspec_k/.
"""

import os
import re

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_CONFIG  = os.path.join(SCRIPT_DIR, 'test.config')
OUT_DIR     = os.path.join(SCRIPT_DIR, '..', 'snapspec_k')
OUT_CONFIG  = os.path.join(OUT_DIR, 'test.config')
OUT_BAT     = os.path.join(OUT_DIR, 'test.bat')


def generate():
    os.makedirs(OUT_DIR, exist_ok=True)

    with open(SRC_CONFIG) as f:
        lines = f.readlines()

    out_lines = []
    for line in lines:
        # Redirect test inputs and reference outputs to the parent snapspec suite
        line = re.sub(r'^(test_dir\s*:\s*)in\s*$',    r'\1../snapspec/in\n',    line)
        line = re.sub(r'^(check_dir\s*:\s*)check\s*$', r'\1../snapspec/check\n', line)
        # Inject -k flag into the snapspec invocation
        line = re.sub(
            r'(command\s*:.*"\{program:snapspec\}")\s+(\{parameters\})',
            r'\1 -k \2',
            line
        )
        out_lines.append(line)

    with open(OUT_CONFIG, 'w') as f:
        f.writelines(out_lines)
        # Strip the KD-tree timing line (only present when -k is used) and
        # ignore the blank line it leaves behind during comparison
        f.write(r'match_replace_re: ~\.(lst)$ ~^\s*\.\.\sSpatial index build.*$~~' + '\n')
        f.write('match_ignore_blanks: yes\n')

    # Windows batch equivalent — identical structure to snapspec/test.bat
    with open(OUT_BAT, 'w') as f:
        f.write('@echo off\nperl ..\\runtests.pl %*\n')


if __name__ == '__main__':
    generate()
