libephysync_sources = [
  'ephy-password-manager.c',
  'ephy-password-record.c',
  'ephy-sync-crypto.c',
  'ephy-sync-service.c',
  'ephy-synchronizable-manager.c',
  'ephy-synchronizable.c'
]

libephysync_deps = [
  glib_dep,
  gmp_dep,
  hogweed_dep,
  json_glib_dep,
  libsecret_dep,
  libsoup_dep,
  nettle_dep,
  webkit2gtk_dep
]

libephysync_includes = include_directories(
  '.',
  '..',
  '../history',
  '../widgets',
  '../..',
  '../../embed'
)

libephysync = static_library('ephysync',
  libephysync_sources,
  dependencies: libephysync_deps,
  include_directories: libephysync_includes
)

ephysync_dep = declare_dependency(
  link_with: libephysync,
  dependencies: libephysync_deps,
  include_directories: libephysync_includes
)
