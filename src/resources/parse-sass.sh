#!/bin/sh

GTK_SOURCE_PATH="../../../gtk+-3"

sass --sourcemap=none --update -I ${GTK_SOURCE_PATH}/gtk/theme/Adwaita .
