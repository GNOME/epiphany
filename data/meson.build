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
  install_dir: join_paths(datadir, 'appdata'),
  po_dir: '../po'
)

# https://github.com/mesonbuild/meson/issues/1687
custom_target('gsettings-enums',
  input: '../lib/ephy-prefs.h',
  output: 'org.gnome.Epiphany.enums.xml',
  install: true,
  install_dir: join_paths(datadir, 'glib-2.0', 'schemas'),
  capture: true,
  command: ['glib-mkenums',
    '--comments', '<!-- @comment@ -->',
    '--fhead', '<schemalist>',
    '--vhead', '  <@type@ id="org.gnome.Epiphany.@EnumName@">',
    '--vprod', '    <value nick="@valuenick@" value="@valuenum@"/>',
    '--vtail', '  </@type@>',
    '--ftail', '</schemalist>',
    '@INPUT@'
  ]
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
