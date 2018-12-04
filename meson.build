project('epiphany', 'c',
  license: 'GPL3+',
  version: '3.31.2',
  meson_version: '>= 0.42.0',
  default_options: ['c_std=gnu11']
)

gnome = import('gnome')
i18n = import('i18n')

prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))
libdir = join_paths(prefix, get_option('libdir'))
libexecdir = join_paths(prefix, get_option('libexecdir'))
liblocaledir = join_paths(prefix, libdir, 'locale')
localedir = join_paths(prefix, get_option('localedir'))
pkgdatadir = join_paths(datadir, 'epiphany')
pkglibdir = join_paths(libdir, 'epiphany')
pkglibexecdir = join_paths(libexecdir, 'epiphany')
webextensionsdir = join_paths(pkglibdir, 'web-extensions')

iso_codes = dependency('iso-codes')
iso_codes_prefix = iso_codes.get_pkgconfig_variable('prefix')

conf = configuration_data()
conf.set_quoted('BUILD_ROOT', meson.build_root())
conf.set_quoted('DISTRIBUTOR_NAME', get_option('distributor_name'))
conf.set_quoted('EPHY_WEB_EXTENSIONS_DIR', webextensionsdir)
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('ISO_CODES_PREFIX', iso_codes_prefix)
conf.set_quoted('LIBLOCALEDIR', liblocaledir)
conf.set_quoted('LOCALEDIR', localedir)
conf.set_quoted('PKGDATADIR', pkgdatadir)
conf.set_quoted('PKGLIBEXECDIR', pkglibexecdir)
conf.set_quoted('SOURCE_ROOT', meson.source_root())

developer_mode = get_option('developer_mode')
conf.set10('DEVELOPER_MODE', developer_mode)

tech_preview = get_option('tech_preview')
conf.set10('TECH_PREVIEW', tech_preview)

conf.set_quoted('VERSION', meson.project_version())

configure_file(
  output: 'config.h',
  configuration: conf
)

if tech_preview or developer_mode
  vcs_tag(
    input: 'ephy-vcs-version.h.in',
    output: 'ephy-vcs-version.h',
    replace_string: '@VCSVERSION@'
  )
else
  version_conf = configuration_data()
  version_conf.set('VCSVERSION', meson.project_version())

  configure_file(
    input: 'ephy-vcs-version.h.in',
    output: 'ephy-vcs-version.h',
    configuration: version_conf
  )
endif

# Dependencies policy: except for WebKitGTK+, all dependency versions must be
# available in Ubuntu 18.04. Please check before bumping the required version
# of any dependency.
glib_requirement = '>= 2.56.0'
gtk_requirement = '>= 3.22.13'
nettle_requirement = '>= 3.4'
webkitgtk_requirement = '>= 2.21.92'

cairo_dep = dependency('cairo', version: '>= 1.2')
gcr_dep = dependency('gcr-3', version: '>= 3.5.5')
gdk_dep = dependency('gdk-3.0', version: gtk_requirement)
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0', version: '>= 2.36.5')
gio_dep = dependency('gio-2.0', version: glib_requirement)
gio_unix_dep = dependency('gio-unix-2.0', version: glib_requirement)
glib_dep = dependency('glib-2.0', version: glib_requirement)
gtk_dep = dependency('gtk+-3.0', version: gtk_requirement)
gtk_unix_print_dep = dependency('gtk+-unix-print-3.0', version: gtk_requirement)
hogweed_dep = dependency('hogweed', version: nettle_requirement)
icu_uc_dep = dependency('icu-uc', version: '>= 4.6')
iso_codes_dep = dependency('iso-codes', version: '>= 0.35')
json_glib_dep = dependency('json-glib-1.0', version: '>= 1.2.4')
libdazzle_dep = dependency('libdazzle-1.0', version: '>= 3.29.4', required: false)
libhandy_dep = dependency('libhandy-0.0', version: '>= 0.0.5', required: false)
libnotify_dep = dependency('libnotify', version: '>= 0.5.1')
libsecret_dep = dependency('libsecret-1', version: '>= 0.14')
libsoup_dep = dependency('libsoup-2.4', version: '>= 2.48.0')
libxml_dep = dependency('libxml-2.0', version: '>= 2.6.12')
nettle_dep = dependency('nettle', version: nettle_requirement)
sqlite3_dep = dependency('sqlite3', version: '>= 3.0')
webkit2gtk_dep = dependency('webkit2gtk-4.0', version: webkitgtk_requirement)
webkit2gtk_web_extension_dep = dependency('webkit2gtk-web-extension-4.0', version: webkitgtk_requirement)

# Ubuntu 18.04 has libdazzle 3.28, the max we can require from the system, but
# we need 3.30, so bundle it if the system copy is too old.
if not libdazzle_dep.found()
  libdazzle_dep = subproject('libdazzle', default_options: ['package_subdir=epiphany',
                                                            'enable_tools=false',
                                                            'with_introspection=false',
                                                            'with_vapi=false']).get_variable('libdazzle_dep')
endif

# Ubuntu 18.04 doesn't have libhandy at all, and we need 0.0.5, so bundle it if
# the system copy is missing or too old.
if not libhandy_dep.found()
  libhandy_dep = subproject('libhandy', default_options: ['examples=false',
                                                          'glade_catalog=false',
                                                          'introspection=false',
                                                          'tests=false',
                                                          'vapi=false']).get_variable('libhandy_dep')
endif

cc = meson.get_compiler('c')
gmp_dep = cc.find_library('gmp')
m_dep = cc.find_library('m', required: false)

subdir('data')
subdir('help')
subdir('po')
subdir('lib')
subdir('embed')
subdir('src')
subdir('tests')

meson.add_install_script('post_install.py')
