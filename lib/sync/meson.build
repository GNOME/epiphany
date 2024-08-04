libephysync_sources = [
  'debug/ephy-sync-debug.c',
  'ephy-history-manager.c',
  'ephy-history-record.c',
  'ephy-open-tabs-manager.c',
  'ephy-open-tabs-record.c',
  'ephy-password-export.c',
  'ephy-password-import.c',
  'ephy-password-manager.c',
  'ephy-password-record.c',
  'ephy-sync-crypto.c',
  'ephy-sync-service.c',
  'ephy-synchronizable-manager.c',
  'ephy-synchronizable.c',
  'ephy-tabs-catalog.c'
]

libephysync_deps = [
  ephymisc_dep,
  glib_dep,
  gmp_dep,
  hogweed_dep,
  json_glib_dep,
  libsecret_dep,
  libsoup_dep,
  nettle_dep,
]

libephysync_includes = include_directories(
  '.',
  '..',
  '../history',
  '../..',
  'debug'
)

libephysync = shared_library('ephysync',
  libephysync_sources,
  dependencies: libephysync_deps,
  include_directories: libephysync_includes,
  install: true,
  install_dir: pkglibdir,
  install_rpath: pkglibdir
)

ephysync_dep = declare_dependency(
  link_with: libephysync,
  dependencies: libephysync_deps,
  include_directories: libephysync_includes
)
