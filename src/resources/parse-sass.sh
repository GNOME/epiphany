#!/bin/sh

GTK_SOURCE_PATH="../../../gtk+"

sass --sourcemap=none --update -I ${GTK_SOURCE_PATH}/gtk/theme/Adwaita .
