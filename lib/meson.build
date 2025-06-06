types_headers = [
  'ephy-adaptive-mode.h',
  'ephy-permissions-manager.h',
  'ephy-prefs.h',
  'ephy-security-levels.h',
  'ephy-sqlite-connection.h',
  'history/ephy-history-types.h'
]

enums = gnome.mkenums_simple('ephy-lib-type-builtins',
  sources: types_headers
)

libephymisc_sources = [
  'contrib/dzl-fuzzy-mutable-index.c',
  'contrib/dzl-suggestion.c',
  'contrib/gnome-languages.c',
  'ephy-debug.c',
  'ephy-favicon-helpers.c',
  'ephy-file-dialog-utils.c',
  'ephy-file-helpers.c',
  'ephy-flatpak-utils.c',
  'ephy-json-utils.c',
  'ephy-langs.c',
  'ephy-notification.c',
  'ephy-notification-container.c',
  'ephy-opensearch-autodiscovery-link.c',
  'ephy-opensearch-engine.c',
  'ephy-output-encoding.c',
  'ephy-permissions-manager.c',
  'ephy-pixbuf-utils.c',
  'ephy-profile-utils.c',
  'ephy-search-engine.c',
  'ephy-search-engine-manager.c',
  'ephy-security-levels.c',
  'ephy-settings.c',
  'ephy-signal-accumulator.c',
  'ephy-smaps.c',
  'ephy-snapshot-service.c',
  'ephy-sqlite-connection.c',
  'ephy-sqlite-statement.c',
  'ephy-string.c',
  'ephy-suggestion.c',
  'ephy-sync-utils.c',
  'ephy-time-helpers.c',
  'ephy-uri-helpers.c',
  'ephy-user-agent.c',
  'ephy-web-app-utils.c',
  'ephy-zoom.c',
  'history/ephy-history-service.c',
  'history/ephy-history-service-hosts-table.c',
  'history/ephy-history-service-urls-table.c',
  'history/ephy-history-service-visits-table.c',
  'history/ephy-history-types.c',
  enums
]

libephymisc_deps = [
  config_h,
  cairo_dep,
  gdk_pixbuf_dep,
  gio_dep,
  gio_unix_dep,
  glib_dep,
  gsettings_desktop_schemas,
  gtk_dep,
  json_glib_dep,
  libadwaita_dep,
  libsecret_dep,
  libsoup_dep,
  libxml_dep,
  m_dep,
  portal_dep,
  sqlite3_dep,
  webkitgtk_dep
]

libephymisc_includes = include_directories(
  '.',
  '..',
  'contrib',
  'history',
)

libephymisc = shared_library('ephymisc',
  libephymisc_sources,
  dependencies: libephymisc_deps,
  include_directories: libephymisc_includes,
  install: true,
  install_dir: pkglibdir,
  install_rpath: pkglibdir
)

ephymisc_dep = declare_dependency(
  link_with: libephymisc,
  include_directories: libephymisc_includes,
  dependencies: libephymisc_deps,
  sources: enums
)

subdir('autofill')
subdir('sync')
