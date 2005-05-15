from xml.dom.minidom import parse, Node, Document, parseString
import xml.parsers.expat

import os
import sys

base = sys.argv[0]
dbfile = os.path.join(os.path.dirname(base), "freedesktop.org.xml")
permissionfile = os.path.join(os.path.dirname(base), "mime-types-permissions.xml")

def PrintIfAbsent(elements, elem):
    for elem2 in elements:
        if (elem.attributes["type"].value == elem2.attributes["type"].value):
	    return;
    print ("<mime-type type=\"" + elem.attributes["type"].value + "\"/>");

def ExtractTypes():
    dbdom = parse(dbfile)
    permissiondom = parse(permissionfile);
    dbelements = dbdom.getElementsByTagName("mime-type") + dbdom.getElementsByTagName("alias")
    permissionelements = permissiondom.getElementsByTagName("mime-type");
    print ("New types:");
    print ("----------\n");
    for elem in dbelements:
        PrintIfAbsent(permissionelements, elem)
    print ("\nTypes removed:");
    print ("--------------\n");
    for elem in permissionelements:
        PrintIfAbsent(dbelements, elem)
    dbdom.unlink();
    permissiondom.unlink();

ExtractTypes();
