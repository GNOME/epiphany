types_headers = [
  'ephy-location-entry.h'
]

enums = gnome.mkenums_simple('ephy-widgets-type-builtins',
  sources: types_headers
)

libephywidgets_sources = [
  'contrib/nautilus-floating-bar.c',
  'ephy-certificate-dialog.c',
  'ephy-downloads-popover.c',
  'ephy-download-widget.c',
  'ephy-file-chooser.c',
  'ephy-location-entry.c',
  'ephy-security-popover.c',
  'ephy-title-box.c',
  'ephy-title-widget.c',
  enums
]

libephywidgets_deps = [
  libdazzle_dep,
  ephymisc_dep,
  gcr_dep,
  gdk_pixbuf_dep,
  gio_dep,
  glib_dep,
  gtk_dep,
  libdazzle_dep,
  libsoup_dep,
  webkit2gtk_dep,
]

libephywidgets_includes = include_directories(
  '.',
  '..',
  '../contrib',
  '../history',
  '../..',
  '../../embed',
  '../sync',
  'contrib'
)

libephywidgets = static_library('ephywidgets',
  libephywidgets_sources,
  dependencies: libephywidgets_deps,
  include_directories: libephywidgets_includes,
)

ephywidgets_dep = declare_dependency(
  link_with: libephywidgets,
  include_directories: libephywidgets_includes,
  dependencies: libephywidgets_deps,
  sources: enums
)
