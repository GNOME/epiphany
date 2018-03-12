resource_files = files('resources/epiphany-web-extension.gresource.xml')
resources = gnome.compile_resources('epiphany-web-extension-resources',
    resource_files,
    c_name: 'epiphany_web_extension',
    source_dir: 'resources'
)

web_extension_sources = [
  'ephy-uri-tester.c',
  'ephy-web-extension.c',
  'ephy-web-extension-main.c',
  'ephy-web-overview.c',
  'ephy-web-overview-model.c',
  resources
]

web_extension_deps = [
  ephymisc_dep,
  ephysync_dep,
  webkit2gtk_web_extension_dep
]

shared_module('ephywebextension',
  web_extension_sources,
  dependencies: web_extension_deps,
  install: true,
  install_dir: webextensionsdir,
  install_rpath: pkglibdir
)
