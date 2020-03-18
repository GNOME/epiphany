pdfjs_resource_files = files('pdfjs/pdfjs.gresource.xml')
pdfjs_resources = gnome.compile_resources('pdfjs-resources',
    pdfjs_resource_files,
    c_name: 'pdfjs',
    source_dir: 'pdfjs'
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
