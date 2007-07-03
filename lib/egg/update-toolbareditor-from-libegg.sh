#!/bin/sh
# update-toolbareditor-from-libegg.sh
#
# Get latest toolbar editor from libegg
# Developers using the toolbar editor in their projects can use this script to
# fetch the latest toolbar editor from libegg. Just run this script
#

SCRIPT_NAME=update-toolbareditor-from-libegg.sh
SVN_URI=http://svn.gnome.org/svn/libegg/trunk/libegg/toolbareditor
FILES="egg-editable-toolbar.c \
       egg-toolbars-model.c \
       egg-toolbar-editor.c \
       eggtreemultidnd.c \
       egg-editable-toolbar.h \
       egg-toolbars-model.h \
       egg-toolbar-editor.h \
       eggtreemultidnd.h \
       eggmarshalers.list" 


if [ -z $1 ]; then
  echo "Obtaining latest version of "$SCRIPT_NAME
  svn export $SVN_URI/$SCRIPT_NAME
  ./$SCRIPT_NAME --update-sources
fi
if  [ "$1"  = "--update-sources" ]; then

  echo "Obtaining latest version of the sources"
  for FILE in $FILES 
  do
    svn export $SVN_URI/$FILE
  done
fi

