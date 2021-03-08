#!/usr/bin/env python3
"""
Pack pdf.js into a gresource file for Epiphany
"""

import os

def create_resource():
    """
    Traverse the current directory and add everything among the first level to the gresource file
    """
    resource = open('pdfjs.gresource.xml', 'w')
    resource.write('<?xml version="1.0" encoding="UTF-8"?>\n')
    resource.write('<gresources>\n')
    resource.write('\t<gresource prefix="/org/gnome/epiphany/pdfjs">\n')

    for root, _, files in os.walk("."):
        for file in files:
            if len(root) > 1:
                resource.write('\t\t<file compressed="true">' + root[2:] + '/' + file + '</file>\n')

    resource.write('\t</gresource>\n')
    resource.write('</gresources>\n')
    resource.close()

if __name__ == "__main__":
    create_resource()
