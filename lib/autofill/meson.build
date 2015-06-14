libephyautofill_sources = [
  'ephy-autofill.c',
  'ephy-autofill-form-element.c',
  'ephy-autofill-input-element.c',
  'ephy-autofill-matchers.c',
  'ephy-autofill-select-element.c',
  'ephy-autofill-storage.c',
  'ephy-autofill-utils.c'
]

libephyautofill_deps = [
  config_h,
  gio_dep,
  glib_dep,
  gtk_dep,
  libsecret_dep,
  libsoup_dep,
  webkit2gtk_dep
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
