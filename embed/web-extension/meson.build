web_extension_sources = [
  'ephy-embed-form-auth.c',
  'ephy-uri-tester.c',
  'ephy-web-dom-utils.c',
  'ephy-web-extension.c',
  'ephy-web-extension-main.c',
  'ephy-web-overview.c',
  'ephy-web-overview-model.c'
]

web_extension_deps = [
  ephymisc_dep,
  ephysync_dep,
  webkit2gtk_web_extension_dep
]

if get_option('https_everywhere')
  web_extension_deps += httpseverywhere_dep
endif

shared_module('ephywebextension',
  web_extension_sources,
  dependencies: web_extension_deps,
  install: true,
  install_dir: webextensionsdir,
  install_rpath: pkglibdir
)
