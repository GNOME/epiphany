types_headers = [
  'bookmarks/ephy-bookmark-properties.h',
  'ephy-link.h',
  'ephy-session.h',
  'ephy-shell.h',
  'ephy-window.h'
]

enums = gnome.mkenums_simple('ephy-type-builtins',
  sources: types_headers
)

subdir('webextension')

libephymain_sources = [
  'bookmarks/ephy-bookmark.c',
  'bookmarks/ephy-bookmark-properties.c',
  'bookmarks/ephy-bookmark-row.c',
  'bookmarks/ephy-bookmarks-dialog.c',
  'bookmarks/ephy-bookmarks-export.c',
  'bookmarks/ephy-bookmarks-import.c',
  'bookmarks/ephy-bookmarks-manager.c',
  'ephy-action-bar.c',
  'ephy-action-bar-end.c',
  'ephy-action-bar-start.c',
  'ephy-action-helper.c',
  'ephy-certificate-dialog.c',
  'ephy-desktop-utils.c',
  'ephy-downloads-paintable.c',
  'ephy-downloads-popover.c',
  'ephy-download-widget.c',
  'ephy-encoding-dialog.c',
  'ephy-encoding-row.c',
  'ephy-firefox-sync-dialog.c',
  'ephy-fullscreen-box.c',
  'ephy-header-bar.c',
  'ephy-history-dialog.c',
  'ephy-link.c',
  'ephy-location-controller.c',
  'ephy-location-entry.c',
  'ephy-lockdown.c',
  'ephy-mouse-gesture-controller.c',
  'ephy-permission-popover.c',
  'ephy-password-popover.c',
  'ephy-privacy-report.c',
  'ephy-security-popover.c',
  'ephy-session.c',
  'ephy-shell.c',
  'ephy-suggestion-model.c',
  'ephy-tab-view.c',
  'ephy-title-box.c',
  'ephy-title-widget.c',
  'ephy-window.c',
  'context-menu-commands.c',
  'preferences/autofill-view.c',
  'preferences/clear-data-view.c',
  'preferences/ephy-data-view.c',
  'preferences/ephy-lang-row.c',
  'preferences/ephy-prefs-dialog.c',
  'preferences/extension-view.c',
  'preferences/passwords-view.c',
  'preferences/ephy-search-engine-listbox.c',
  'preferences/ephy-search-engine-row.c',
  'preferences/prefs-appearance-page.c',
  'preferences/prefs-extensions-page.c',
  'preferences/prefs-general-page.c',
  'preferences/prefs-privacy-page.c',
  'preferences/webapp-additional-urls-dialog.c',
  'preferences/webapp-additional-urls-list-item.c',
  'synced-tabs-dialog.c',
  'window-commands.c',
  ephywebextension_src,
  compile_schemas,
  enums
]

libephymain_deps = [
  config_h,
  ephyembed_dep,
  ephymisc_dep,
  ephysync_dep,
  gcr_dep,
  gdk_pixbuf_dep,
  granite_dep,
  gstreamer_dep,
  gvdb_dep,
  libarchive_dep,
  libadwaita_dep,
  libsoup_dep,
  portal_dep,
  webkitgtk_dep,
]

libephymain_includes = include_directories(
  '.',
  '..',
  '../lib/contrib',
  'bookmarks',
  'preferences',
  'webextension',
  'webextension/api',
)

libephymain = shared_library('ephymain',
  libephymain_sources,
  dependencies: libephymain_deps,
  include_directories: libephymain_includes,
  install: true,
  install_dir: pkglibdir,
  install_rpath: pkglibdir
)

ephymain_dep = declare_dependency(
  link_with: libephymain,
  include_directories: libephymain_includes,
  dependencies: libephymain_deps,
  sources: enums
)


ephy_profile_migrator = executable('ephy-profile-migrator',
  'profile-migrator/ephy-legacy-web-app-utils.c',
  'profile-migrator/ephy-profile-migrator.c',
  dependencies: ephymain_dep,
  install: true,
  install_dir: pkglibexecdir,
  install_rpath: pkglibdir
)


search_provider_codegen = gnome.gdbus_codegen('ephy-shell-search-provider-generated',
  'search-provider/org.gnome.ShellSearchProvider2.xml',
  interface_prefix: 'org.gnome',
  namespace: 'Ephy'
)

search_provider_sources = [
  'search-provider/ephy-search-provider.c',
  'search-provider/ephy-search-provider-main.c',
  search_provider_codegen
]

executable('epiphany-search-provider',
  search_provider_sources,
  dependencies: ephymain_dep,
  install: true,
  install_dir: libexecdir,
  install_rpath: pkglibdir
)

webapp_codegen = gnome.gdbus_codegen('ephy-webapp-provider-generated',
  'webapp-provider/org.gnome.Epiphany.WebAppProvider.xml',
  interface_prefix: 'org.gnome.Epiphany',
  namespace: 'Ephy'
)

webapp_provider_sources = [
  'webapp-provider/ephy-webapp-provider.c',
  'webapp-provider/ephy-webapp-provider-main.c',
  webapp_codegen
]

executable('epiphany-webapp-provider',
  webapp_provider_sources,
  dependencies: ephymain_dep,
  install: true,
  install_dir: libexecdir,
  install_rpath: pkglibdir
)

resource_files = files('resources/epiphany.gresource.xml')
resources = gnome.compile_resources('epiphany-resources',
    resource_files,
    c_name: 'epiphany',
    source_dir: 'resources'
)

epiphany_sources = [
  'ephy-main.c',
  resources,
  highlightjs_resources,
  readability_resources
]

epiphany = executable('epiphany',
  epiphany_sources,
  dependencies: ephymain_dep,
  install: true,
  install_rpath: pkglibdir
)
