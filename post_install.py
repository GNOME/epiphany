#!/usr/bin/python3

import os
import pathlib
import subprocess

prefix = pathlib.Path(os.environ.get('MESON_INSTALL_PREFIX', '/usr/local'))
datadir = prefix / 'share'
destdir = os.environ.get('DESTDIR', '')

if not destdir:
    print('Compiling gsettings schemas...')
    subprocess.call(['glib-compile-schemas', str(datadir / 'glib-2.0' / 'schemas')])

    print('Updating icon cache...')
    subprocess.call(['gtk-update-icon-cache', '-qtf', str(datadir / 'icons' / 'hicolor')])

    print('Updating desktop database...')
    subprocess.call(['update-desktop-database', '-q', str(datadir / 'applications')])
