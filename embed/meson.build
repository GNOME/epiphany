subdir('web-extension')

types_headers = [
  'ephy-download.h',
  'ephy-embed-shell.h',
  'ephy-web-view.h'
]

enums = gnome.mkenums_simple('ephy-embed-type-builtins',
  sources: types_headers
)

libephyembed_sources = [
  'ephy-about-handler.c',
  'ephy-downloads-manager.c',
  'ephy-download.c',
  'ephy-embed.c',
  'ephy-embed-container.c',
  'ephy-embed-event.c',
  'ephy-embed-prefs.c',
  'ephy-embed-shell.c',
  'ephy-embed-utils.c',
  'ephy-encoding.c',
  'ephy-encodings.c',
  'ephy-evince-document-view.c',
  'ephy-file-monitor.c',
  'ephy-filters-manager.c',
  'ephy-find-toolbar.c',
  'ephy-view-source-handler.c',
  'ephy-web-view.c',
  'ephy-web-extension-proxy.c',
  enums
]

libephyembed_deps = [
  libdazzle_dep,
  ephymisc_dep,
  evince_document_dep,
  evince_view_dep,
  gio_dep,
  glib_dep,
  gtk_dep,
  icu_uc_dep,
  libsecret_dep,
  libsoup_dep,
  m_dep,
  webkit2gtk_dep,
]

libephyembed_includes = include_directories(
  '.',
  '..',
  '../lib',
  '../lib/contrib',
  '../lib/contrib/gvdb',
  '../lib/history',
  '../lib/sync',
  '../lib/widgets',
  '../lib/widgets/contrib'
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
