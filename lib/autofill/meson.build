libephyautofill_sources = [
  'ephy-autofill-storage.c'
]

libephyautofill_deps = [
  config_h,
  gio_dep,
  glib_dep,
  gtk_dep,
  libsecret_dep,
  libsoup_dep,
  webkitgtk_web_extension_dep
]

libephyautofill_includes = include_directories(
  '.',
  '../..'
)

libephyautofill = shared_library('ephyautofill',
  libephyautofill_sources,
  dependencies: libephyautofill_deps,
  include_directories: libephyautofill_includes,
  install: true,
  install_dir: pkglibdir
)

ephyautofill_dep = declare_dependency(
  link_with: libephyautofill,
  dependencies: libephyautofill_deps,
  include_directories: libephyautofill_includes
)
