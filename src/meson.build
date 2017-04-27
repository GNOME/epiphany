types_headers = [
  'bookmarks/ephy-bookmark-properties-grid.h',
  'ephy-link.h',
  'ephy-session.h',
  'ephy-shell.h',
  'ephy-window.h'
]

enums = gnome.mkenums('ephy',
  sources: types_headers,
  h_template: 'ephy-type-builtins.h.in',
  c_template: 'ephy-type-builtins.c.in'
)

libephymain_sources = [
  'bookmarks/ephy-add-bookmark-popover.c',
  'bookmarks/ephy-bookmark.c',
  'bookmarks/ephy-bookmark-properties-grid.c',
  'bookmarks/ephy-bookmark-row.c',
  'bookmarks/ephy-bookmarks-export.c',
  'bookmarks/ephy-bookmarks-import.c',
  'bookmarks/ephy-bookmarks-manager.c',
  'bookmarks/ephy-bookmarks-popover.c',
  'clear-data-dialog.c',
  'cookies-dialog.c',
  'ephy-action-helper.c',
  'ephy-completion-model.c',
  'ephy-encoding-dialog.c',
  'ephy-encoding-row.c',
  'ephy-header-bar.c',
  'ephy-history-dialog.c',
  'ephy-link.c',
  'ephy-location-controller.c',
  'ephy-lockdown.c',
  'ephy-notebook.c',
  'ephy-search-engine-dialog.c',
  'ephy-session.c',
  'ephy-shell.c',
  'ephy-window.c',
  'passwords-dialog.c',
  'popup-commands.c',
  'prefs-dialog.c',
  'window-commands.c',
  enums
]

libephymain_deps = [
  ephyembed_dep,
  ephymisc_dep,
  ephywidgets_dep,
  json_glib_dep,
  libnotify_dep
]

libephymain_includes = include_directories(
  '.',
  'bookmarks',
  'sync'
)

if get_option('enable_firefox_sync')
  libephymain_sources += [
    'sync/ephy-sync-crypto.c',
    'sync/ephy-sync-secret.c',
    'sync/ephy-sync-service.c',
    'sync/ephy-sync-utils.c'
  ]

  libephymain_deps += [
    gmp_dep,
    hogweed_dep,
    nettle_dep
  ]
endif

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


executable('ephy-profile-migrator',
  'profile-migrator/ephy-profile-migrator.c',
  dependencies: ephymain_dep,
  install: true,
  install_dir: pkglibexecdir,
  install_rpath: pkglibdir
)


codegen = gnome.gdbus_codegen('ephy-shell-search-provider-generated',
  'search-provider/org.gnome.ShellSearchProvider2.xml',
  interface_prefix: 'org.gnome',
  namespace: 'Ephy'
)

search_provider_sources = [
  'search-provider/ephy-search-provider.c',
  'search-provider/ephy-search-provider-main.c',
  codegen
]

executable('epiphany-search-provider',
  search_provider_sources,
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
  resources
]

executable('epiphany',
  epiphany_sources,
  dependencies: ephymain_dep,
  install: true,
  install_rpath: pkglibdir
)
