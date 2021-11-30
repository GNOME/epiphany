po_dir = join_paths(meson.source_root(), 'po')

icondir = join_paths('icons', 'hicolor', 'scalable', 'apps')
install_data(
  join_paths(icondir, ('@0@.svg').format(application_id)),
  install_dir: join_paths(datadir, icondir),
)

icondir = join_paths('icons', 'hicolor', 'symbolic', 'apps')
install_data(
  join_paths(icondir, 'org.gnome.Epiphany-symbolic.svg'),
  install_dir: join_paths(datadir, icondir),
  rename: '@0@-symbolic.svg'.format(application_id)
)

desktop_conf = configuration_data()
desktop_conf.set('icon', application_id)
desktop = i18n.merge_file(
  input: configure_file(
    input: files('org.gnome.Epiphany.desktop.in.in'),
    output: 'org.gnome.Epiphany.desktop.in',
    configuration: desktop_conf
  ),
  output: '@0@.desktop'.format(application_id),
  install: true,
  install_dir: desktopdir,
  po_dir: po_dir,
  type: 'desktop'
)

appdata_conf = configuration_data()
appdata_conf.set('appid', application_id)
appdata = i18n.merge_file(
  input: configure_file(
    input: files('org.gnome.Epiphany.appdata.xml.in.in'),
    output: 'org.gnome.Epiphany.appdata.xml.in',
    configuration: appdata_conf
  ),
  output: '@0@.appdata.xml'.format(application_id),
  install: true,
  install_dir: join_paths(datadir, 'metainfo'),
  po_dir: po_dir
)
appstream_util = find_program('appstream-util', required: false)

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

service_conf = configuration_data()
service_conf.set('appid', application_id)
service_conf.set('libexecdir', libexecdir)
configure_file(
  input: 'org.gnome.Epiphany.SearchProvider.service.in',
  output: '@0@.SearchProvider.service'.format(application_id),
  configuration: service_conf,
  install_dir: servicedir
)

configure_file(
  input: 'org.gnome.Epiphany.WebAppProvider.service.in',
  output: 'org.gnome.Epiphany.WebAppProvider.service',
  configuration: service_conf,
  install_dir: servicedir
)

search_provider_conf = configuration_data()
search_provider_conf.set('appid', application_id)
search_provider_conf.set('profile', profile != '' ? '/' + profile : '')
configure_file(
  configuration: search_provider_conf,
  input: files('org.gnome.Epiphany.SearchProvider.ini.in'),
  install_dir: join_paths(datadir, 'gnome-shell', 'search-providers'),
  output: '@0@.SearchProvider.ini'.format(application_id)
)

bookmarksconf = configuration_data()
bookmarksconf.set('pkgdatadir', pkgdatadir)
configure_file(
  input: 'default-bookmarks.rdf.in',
  output: 'default-bookmarks.rdf',
  configuration: bookmarksconf,
  install_dir: pkgdatadir
)

install_man('epiphany.1')

desktop_file_validate = find_program('desktop-file-validate', required: false)
if desktop_file_validate.found()
  test(
    'validate-desktop',
    desktop_file_validate,
    args: desktop.full_path(),
    depends: desktop
  )
endif

appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  test(
    'validate-appdata', appstream_util,
    args: [
      'validate-relax', '--nonet', appdata.full_path()
    ],
    depends: appdata
  )
endif
