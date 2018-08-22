#!/bin/sh

if [ ! "$(which sassc 2> /dev/null)" ]; then
   echo sassc needs to be installed to generate the css.
   exit 1
fi

SASSC_OPT="-M -t compact"

: ${GTK_SOURCE_PATH:="../../../gtk+-3"}

sassc $SASSC_OPT -I${GTK_SOURCE_PATH}/gtk/theme/Adwaita \
	themes/Adwaita.scss themes/Adwaita.css
sassc $SASSC_OPT -I${GTK_SOURCE_PATH}/gtk/theme/Adwaita \
	themes/shared.scss themes/shared.css
