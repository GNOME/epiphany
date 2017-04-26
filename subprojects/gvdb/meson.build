project('gvdb', 'c',
  meson_version: '>= 0.40.0',
  default_options: ['static=true'],
)

gvdb_sources = [
  'gvdb-builder.c',
  'gvdb-reader.c'
]

glib_dep = dependency('glib-2.0')

gvdb_lib = static_library('gvdb',
  gvdb_sources,
  dependencies: glib_dep
)

gvdb_dep = declare_dependency(
  link_with: gvdb_lib,
  include_directories: include_directories('.'),
  dependencies: glib_dep
)
