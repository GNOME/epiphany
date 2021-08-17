subdir('web-process-extension')

types_headers = [
  'ephy-download.h',
  'ephy-embed-shell.h',
  'ephy-search-entry.h',
  'ephy-web-view.h'
]

enums = gnome.mkenums_simple('ephy-embed-type-builtins',
  sources: types_headers
)

libephyembed_sources = [
  'ephy-about-handler.c',
  'ephy-client-certificate-manager.c',
  'ephy-downloads-manager.c',
  'ephy-download.c',
  'ephy-embed.c',
  'ephy-embed-autofill.c',
  'ephy-embed-container.c',
  'ephy-embed-prefs.c',
  'ephy-embed-shell.c',
  'ephy-embed-utils.c',
  'ephy-encoding.c',
  'ephy-encodings.c',
  'ephy-file-monitor.c',
  'ephy-filters-manager.c',
  'ephy-find-toolbar.c',
  'ephy-floating-bar.c',
  'ephy-reader-handler.c',
  'ephy-search-entry.c',
  'ephy-view-source-handler.c',
  'ephy-web-view.c',
  enums
]

libephyembed_deps = [
  config_h,
  ephyautofill_dep,
  ephymisc_dep,
  gck_dep,
  gcr_dep,
  gio_dep,
  glib_dep,
  gtk_dep,
  libadwaita_dep,
  libsecret_dep,
  libsoup_dep,
  m_dep,
  webkitgtk_dep
]

libephyembed_includes = include_directories(
  '.',
  '..',
  '../lib',
  '../lib/autofill',
  '../lib/contrib',
  '../lib/history',
  '../lib/sync',
)

libephyembed = static_library('ephyembed',
  libephyembed_sources,
  dependencies: libephyembed_deps,
  include_directories: libephyembed_includes,
)

ephyembed_dep = declare_dependency(
  link_with: libephyembed,
  include_directories: libephyembed_includes,
  dependencies: libephyembed_deps,
  sources: enums
)
