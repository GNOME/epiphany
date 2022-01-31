project('epiphany', 'c',
  license: 'GPL3+',
  version: '42.beta',
  meson_version: '>= 0.51.0',
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
conf.set_quoted('PKGLIBEXECDIR', pkglibexecdir)
conf.set_quoted('PROFILE', profile)
conf.set_quoted('SOURCE_ROOT', meson.source_root())
tech_preview = get_option('tech_preview')
conf.set10('TECH_PREVIEW', tech_preview)
conf.set_quoted('G_LOG_DOMAIN', 'epiphany')

version_array = meson.project_version().split('.')
conf.set('EPHY_MAJOR_VERSION', version_array[0].to_int())
if version_array[1] in ['alpha', 'beta', 'rc']
  conf.set('EPHY_MINOR_VERSION', 0)
else
  conf.set('EPHY_MINOR_VERSION', version_array[1].to_int())
endif
conf.set('EPHY_MICRO_VERSION', 0)

conf.set_quoted('VERSION', '@VCS_TAG@')

gsb_api_key = get_option('gsb_api_key')
conf.set_quoted('GSB_API_KEY', gsb_api_key)
conf.set10('ENABLE_GSB', gsb_api_key != '')

glib_requirement = '>= 2.67.4'
gtk_requirement = '>= 3.24.0'
nettle_requirement = '>= 3.4'
webkitgtk_requirement = '>= 2.33.2'

cairo_dep = dependency('cairo', version: '>= 1.2')
gcr_dep = dependency('gcr-3', version: '>= 3.5.5')
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0', version: '>= 2.36.5')
gio_dep = dependency('gio-2.0', version: glib_requirement)
gio_unix_dep = dependency('gio-unix-2.0', version: glib_requirement)
glib_dep = dependency('glib-2.0', version: glib_requirement)
gsettings_desktop_schemas = dependency('gsettings-desktop-schemas')
gtk_dep = dependency('gtk+-3.0', version: gtk_requirement)
gtk_unix_print_dep = dependency('gtk+-unix-print-3.0', version: gtk_requirement)
hogweed_dep = dependency('hogweed', version: nettle_requirement)
iso_codes_dep = dependency('iso-codes', version: '>= 0.35')
json_glib_dep = dependency('json-glib-1.0', version: '>= 1.6')
libarchive_dep = dependency('libarchive')
libdazzle_dep = dependency('libdazzle-1.0', version: '>= 3.37.1')
libhandy_dep = dependency('libhandy-1', version: '>= 1.5.0')
libsecret_dep = dependency('libsecret-1', version: '>= 0.19.0')
libxml_dep = dependency('libxml-2.0', version: '>= 2.6.12')
nettle_dep = dependency('nettle', version: nettle_requirement)
portal_dep = dependency('libportal-gtk3', version: '>= 0.6')
sqlite3_dep = dependency('sqlite3', version: '>= 3.22')

if get_option('soup2').enabled()
  libsoup_dep = dependency('libsoup-2.4', version: '>= 2.48.0')
  webkit2gtk_dep = dependency('webkit2gtk-4.0', version: webkitgtk_requirement)
  webkit2gtk_web_extension_dep = dependency('webkit2gtk-web-extension-4.0', version: webkitgtk_requirement)
else
  libsoup_dep = dependency('libsoup-3.0', version: '>= 2.99.4')
  webkit2gtk_dep = dependency('webkit2gtk-4.1', version: webkitgtk_requirement)
  webkit2gtk_web_extension_dep = dependency('webkit2gtk-web-extension-4.1', version: webkitgtk_requirement)
endif

webkit_revision = webkit2gtk_dep.get_variable(pkgconfig : 'revision', default_value : '')
if webkit_revision == 'tarball'
  webkit_revision = ''
elif webkit_revision != ''
  template = ' (@0@)'
  webkit_revision = template.format(webkit_revision)
endif
conf.set_quoted('WEBKIT_REVISION', webkit_revision)

config_h = declare_dependency(
  sources: vcs_tag(
    input: configure_file(
      output: 'config.h.in',
      configuration: conf
    ),
    output: 'config.h'
  )
)

cc = meson.get_compiler('c')

mini_gmp_test = '''
#include <nettle/bignum.h>

#if !NETTLE_USE_MINI_GMP
# error "gmp is needed"
#endif
'''

if cc.compiles(mini_gmp_test, dependencies: hogweed_dep)
  gmp_dep = dependency('', required: false)
else
  gmp_dep = cc.find_library('gmp')
endif

m_dep = cc.find_library('m', required: false)

cflags = cc.get_supported_arguments(['-Werror=declaration-after-statement',
                                     '-Werror=implicit-function-declaration',
                                     '-Wno-cast-function-type',
                                     '-Wno-missing-field-initializers',
                                     '-Wno-unused-parameter'])
add_project_arguments(cflags,
                      language: 'c')

subdir('data')
subdir('help')
subdir('po')
subdir('third-party')
subdir('lib')
subdir('embed')
subdir('src')
subdir('tests')

meson.add_install_script('post_install.py')

