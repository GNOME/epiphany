#
# Web Process Extensions
#

web_process_extension_resource_files = files('resources/epiphany-web-process-extension.gresource.xml')
web_process_extension_resources = gnome.compile_resources('epiphany-web-process-extension-resources',
    web_process_extension_resource_files,
    c_name: 'epiphany_web_process_extension',
    source_dir: 'resources'
)

web_extension_common_sources = [
  'ephy-webextension-common.c'
]

web_process_extension_sources = [
  'ephy-web-process-extension.c',
  'ephy-web-process-extension-main.c',
  'ephy-web-overview-model.c',
  web_process_extension_resources,
  web_extension_common_sources,
]

web_process_extension_deps = [
  config_h,
  ephyautofill_dep,
  ephymisc_dep,
  ephysync_dep,
  webkitgtk_web_extension_dep
]

shared_module('ephywebprocessextension',
  web_process_extension_sources,
  dependencies: web_process_extension_deps,
  install: true,
  install_dir: webprocessextensionsdir,
  install_rpath: pkglibdir
)
