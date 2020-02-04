#!/usr/bin/env python3

"""
Generates C source file with GLSL input file contents as C string.
"""

import argparse

def main(infile, outfile):

    with open(infile, 'r') as glsl, open(outfile, 'w') as cstr:
        cstr.write("const char *glsl =\n")
        for line in glsl.readlines():
            cstr.write("\"{0}\\n\"\n".format(line.rstrip()))
        cstr.write(";")

if __name__ == '__main__':
    from argparse import ArgumentParser

    parser = ArgumentParser()
    parser.add_argument("infile")
    parser.add_argument("outfile")
    args = parser.parse_args()

    main(args.infile, args.outfile)
