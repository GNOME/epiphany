highlightjs_resource_files = files('highlightjs/highlightjs.gresource.xml')
highlightjs_resources = gnome.compile_resources('highlightjs-resources',
    highlightjs_resource_files,
    c_name: 'highlightjs',
    source_dir: 'highlightjs'
)

readability_resource_files = files('readability/readability.gresource.xml')
readability_resources = gnome.compile_resources('readability-resources',
    readability_resource_files,
    c_name: 'readability',
    source_dir: 'readability'
)

libgvdb_sources = [
  'gvdb/gvdb-builder.c',
  'gvdb/gvdb-reader.c'
]

libgvdb_deps = [
  gio_dep,
  glib_dep
]

libgvdb_includes = [
  'gvdb/'
]

libgvdb = static_library('gvdb',
  libgvdb_sources,
  dependencies: libgvdb_deps,
  include_directories: libgvdb_includes
)

gvdb_dep = declare_dependency(
  link_with: libgvdb,
  include_directories: libgvdb_includes,
  dependencies: libgvdb_deps
)
