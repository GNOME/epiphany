project('epiphany', 'c',
  license: 'GPL3+',
  version: '3.33.1',
  meson_version: '>= 0.46.0',
  default_options: ['c_std=gnu11',
                    'warning_level=2']
)

gnome = import('gnome')
i18n = import('i18n')

r = run_command('grep', '-Po', '^NAME=\K.*', '/etc/os-release')
if r.returncode() == 0
  distributor_name = r.stdout().strip()
else
  distributor_name = 'GNOME Web'
endif

prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))
desktopdir = join_paths(datadir, 'applications')
libdir = join_paths(prefix, get_option('libdir'))
libexecdir = join_paths(prefix, get_option('libexecdir'))
liblocaledir = join_paths(prefix, libdir, 'locale')
localedir = join_paths(prefix, get_option('localedir'))
pkgdatadir = join_paths(datadir, 'epiphany')
pkglibdir = join_paths(libdir, 'epiphany')
pkglibexecdir = join_paths(libexecdir, 'epiphany')
servicedir = join_paths(datadir, 'dbus-1', 'services')
webprocessextensionsdir = join_paths(pkglibdir, 'web-process-extensions')

iso_codes = dependency('iso-codes')
iso_codes_prefix = iso_codes.get_pkgconfig_variable('prefix')

profile = get_option('profile')
application_id = 'org.gnome.Epiphany'

if profile != ''
  application_id = 'org.gnome.Epiphany.' + profile
endif

conf = configuration_data()
conf.set_quoted('APPLICATION_ID', application_id)
conf.set_quoted('BUILD_ROOT', meson.build_root())
developer_mode = get_option('developer_mode')
conf.set10('DEVELOPER_MODE', developer_mode)
conf.set_quoted('DISTRIBUTOR_NAME', distributor_name)
conf.set_quoted('EPHY_WEB_PROCESS_EXTENSIONS_DIR', webprocessextensionsdir)
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('ISO_CODES_PREFIX', iso_codes_prefix)
conf.set_quoted('LIBLOCALEDIR', liblocaledir)
conf.set_quoted('LOCALEDIR', localedir)
conf.set_quoted('PKGDATADIR', pkgdatadir)
conf.set_quoted('PKGLIBEXECDIR', pkglibexecdir)
conf.set_quoted('PROFILE', profile)
conf.set_quoted('SOURCE_ROOT', meson.source_root())
tech_preview = get_option('tech_preview')
conf.set10('TECH_PREVIEW', tech_preview)

version_array = meson.project_version().split('.')
conf.set('EPHY_MAJOR_VERSION', version_array[0].to_int())
conf.set('EPHY_MINOR_VERSION', version_array[1].to_int())
conf.set('EPHY_MICRO_VERSION', version_array[2].to_int())

conf.set_quoted('VERSION', '@0@-@VCS_TAG@'.format(meson.project_version()))

config_h = declare_dependency(
  sources: vcs_tag(
    command: ['git', 'rev-parse', '--short', 'HEAD'],
    fallback: get_option('profile') != 'default' ? 'devel' : 'stable',
    input: configure_file(
      output: 'config.h.in',
      configuration: conf
    ),
    output: 'config.h'
  )
)

glib_requirement = '>= 2.56.0'
gtk_requirement = '>= 3.24.0'
nettle_requirement = '>= 3.4'
webkitgtk_requirement = '>= 2.24.1'

cairo_dep = dependency('cairo', version: '>= 1.2')
gcr_dep = dependency('gcr-3', version: '>= 3.5.5')
gdk_dep = dependency('gdk-3.0', version: gtk_requirement)
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0', version: '>= 2.36.5')
gio_dep = dependency('gio-2.0', version: glib_requirement)
gio_unix_dep = dependency('gio-unix-2.0', version: glib_requirement)
glib_dep = dependency('glib-2.0', version: glib_requirement)
gsettings_desktop_schemas = dependency('gsettings-desktop-schemas')
gtk_dep = dependency('gtk+-3.0', version: gtk_requirement)
gtk_unix_print_dep = dependency('gtk+-unix-print-3.0', version: gtk_requirement)
hogweed_dep = dependency('hogweed', version: nettle_requirement)
iso_codes_dep = dependency('iso-codes', version: '>= 0.35')
json_glib_dep = dependency('json-glib-1.0', version: '>= 1.2.4')
libdazzle_dep = dependency('libdazzle-1.0', version: '>= 3.31.90')
libhandy_dep = dependency('libhandy-0.0', version: '>= 0.0.9', required: false)
libnotify_dep = dependency('libnotify', version: '>= 0.5.1')
libsecret_dep = dependency('libsecret-1', version: '>= 0.14')
libsoup_dep = dependency('libsoup-2.4', version: '>= 2.48.0')
libxml_dep = dependency('libxml-2.0', version: '>= 2.6.12')
nettle_dep = dependency('nettle', version: nettle_requirement)
sqlite3_dep = dependency('sqlite3', version: '>= 3.0')
webkit2gtk_dep = dependency('webkit2gtk-4.0', version: webkitgtk_requirement)
webkit2gtk_web_extension_dep = dependency('webkit2gtk-web-extension-4.0', version: webkitgtk_requirement)

# Ubuntu 18.04 doesn't have libhandy at all, and we need 0.0.5, so bundle it if
# the system copy is missing or too old.
if not libhandy_dep.found()
  libhandy_dep = subproject('libhandy', default_options: ['examples=false',
                                                          'glade_catalog=disabled',
                                                          'introspection=disabled',
                                                          'package_subdir=epiphany',
                                                          'static=true',
                                                          'tests=false',
                                                          'vapi=false']).get_variable('libhandy_dep')
endif

cc = meson.get_compiler('c')
gmp_dep = cc.find_library('gmp')
m_dep = cc.find_library('m', required: false)

cflags = cc.get_supported_arguments(['-Werror=format-security',
                                     '-Wno-cast-function-type',
                                     '-Wno-missing-field-initializers',
                                     '-Wno-unused-parameter'])
add_project_arguments(cflags,
                      language: 'c')

subdir('data')
subdir('help')
subdir('po')
subdir('lib')
subdir('embed')
subdir('src')
subdir('tests')

meson.add_install_script('post_install.py')
