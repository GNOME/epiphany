#!/bin/sh

# Aim for a size of 1280x720, but wmctrl doesn't seem to take into account
# the size of the window borders, 52x52. Note: must be run under X11.
wmctrl -x -r epiphany.Epiphany -e 0,-1,-1,1332,772
wmctrl -x -a epiphany.Epiphany
