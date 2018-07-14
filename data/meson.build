install_subdir('icons/hicolor',
  install_dir: join_paths(datadir, 'icons')
)

i18n.merge_file('desktop-file',
  input: 'org.gnome.Epiphany.desktop.in',
  output: 'org.gnome.Epiphany.desktop',
  install: true,
  install_dir: join_paths(datadir, 'applications'),
  po_dir: '../po',
  type: 'desktop'
)

i18n.merge_file('appdata-file',
  input: 'org.gnome.Epiphany.appdata.xml.in',
  output: 'org.gnome.Epiphany.appdata.xml',
  install: true,
  install_dir: join_paths(datadir, 'metainfo'),
  po_dir: '../po'
)

# We need three custom_target() calls because Meson doesn't have very
# good support for GSettings yet. First, generate our GSettings enums
# in the build directory. Then, copy the gschema into the build
# directory to ensure it is in the same place as the enums file. Then
# we can run glib-compile-schemas on it. We have to compile schemas in
# the build directory to ensure that our tests don't fail if run on a
# system where epiphany is not already installed. epiphany will also
# look for schemas here if built in developer mode.
#
# https://github.com/mesonbuild/meson/issues/1687
generate_enums = custom_target('gsettings-enums',
  input: '../lib/ephy-prefs.h',
  output: 'org.gnome.Epiphany.enums.xml',
  install: true,
  install_dir: join_paths(datadir, 'glib-2.0', 'schemas'),
  capture: true,
  command: [find_program('glib-mkenums'),
    '--comments', '<!-- @comment@ -->',
    '--fhead', '<schemalist>',
    '--vhead', '  <@type@ id="org.gnome.Epiphany.@EnumName@">',
    '--vprod', '    <value nick="@valuenick@" value="@valuenum@"/>',
    '--vtail', '  </@type@>',
    '--ftail', '</schemalist>',
    '@INPUT@'
  ]
)
copy_gschema = custom_target('copy-gschema-to-builddir',
  input: 'org.gnome.epiphany.gschema.xml',
  output: 'renamed-hack.gschema.xml',
  command: ['cp', '@INPUT@', '@OUTPUT@']
)
compile_schemas = custom_target('glib-compile-schemas',
  output: 'gschemas.compiled',
  install: false,
  command: [find_program('glib-compile-schemas'),
    meson.current_build_dir()
  ],
  depends: [generate_enums, copy_gschema]
)
install_data('org.gnome.epiphany.gschema.xml',
  install_dir: join_paths(datadir, 'glib-2.0', 'schemas')
)

serviceconf = configuration_data()
serviceconf.set('libexecdir', libexecdir)
configure_file(
  input: 'org.gnome.Epiphany.SearchProvider.service.in',
  output: 'org.gnome.Epiphany.SearchProvider.service',
  configuration: serviceconf,
  install: true,
  install_dir: join_paths(datadir, 'dbus-1', 'services')
)
install_data('org.gnome.Epiphany.search-provider.ini',
  install_dir: join_paths(datadir, 'gnome-shell', 'search-providers')
)

bookmarksconf = configuration_data()
bookmarksconf.set('pkgdatadir', pkgdatadir)
configure_file(
  input: 'default-bookmarks.rdf.in',
  output: 'default-bookmarks.rdf',
  configuration: bookmarksconf,
  install: true,
  install_dir: pkgdatadir
)

install_man('epiphany.1')
