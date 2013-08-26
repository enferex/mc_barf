#!/usr/bin/env python
import os, re, struct, sys


def usage():
    print('Usage: ' + sys.argv[0] + ' <intel microcode .dat file>')
    sys.exit(0)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        usage()

    # Must be .dat file
    fname = sys.argv[1]
    if fname.find('.dat') == -1:
        print('File ' + fname + ' is not an Intel ascii .dat file')
        sys.exit(-1)

    # Turn fname.dat to fname.bin
    out_fname = fname.split('.dat')[0] + '.bin'

    # Exists
    if os.path.exists(out_fname):
        print('File ' + out_fname + ' exists.  I will not overwrite this!')
        sys.exit(-1)
    if os.path.exists(fname) is False:
        print('Cannot locate microcode file ' + fname)
        sys.exit(-1)

    # Open/create brand spankin new .bin file
    try:
        out = open(out_fname, 'wb')
    except Exception as ex:
        print('Could not open ' + out_fname + ': ' + ex)

    # Ignore lines with /* or // 
    for line in open(fname):
        if line.find('/') is not -1:
            continue
        vals = [x.strip(', \t\n') for x in line.split()]
        for v in vals:
            s = struct.pack('I', int(v, 16))
            out.write(s)

    out.close()
