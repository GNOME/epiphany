types_headers = [
  'ephy-location-entry.h'
]

widget_enums = gnome.mkenums('ephy-widgets',
  sources: types_headers,
  h_template: 'ephy-widgets-type-builtins.h.in',
  c_template: 'ephy-widgets-type-builtins.c.in'
)

libephywidgets_sources = [
  'contrib/gd-two-lines-renderer.c',
  'contrib/nautilus-floating-bar.c',
  'ephy-certificate-dialog.c',
  'ephy-downloads-popover.c',
  'ephy-downloads-progress-icon.c',
  'ephy-download-widget.c',
  'ephy-file-chooser.c',
  'ephy-location-entry.c',
  'ephy-middle-clickable-button.c',
  'ephy-security-popover.c',
  'ephy-title-box.c',
  'ephy-title-widget.c',
  widget_enums
]

libephywidgets_deps = [
  gcr_dep,
  gdk_dep,
  gdk_pixbuf_dep,
  gio_dep,
  glib_dep,
  gtk_dep,
  libsoup_dep,
  webkit2gtk_dep
]

libephywidgets_includes = include_directories(
  '.',
  '..',
  '../contrib',
  '../history',
  '../..',
  '../../embed',
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
  sources: [
    ephy_lib_type_builtins_h,
    widget_enums
  ]
)
