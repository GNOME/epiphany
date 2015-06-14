types_headers = [
  'bookmarks/ephy-bookmark-properties-grid.h',
  'ephy-link.h',
  'ephy-session.h',
  'ephy-shell.h',
  'ephy-window.h'
]

enums = gnome.mkenums_simple('ephy-type-builtins',
  sources: types_headers
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
  'ephy-action-bar.c',
  'ephy-action-bar-end.c',
  'ephy-action-bar-start.c',
  'ephy-action-helper.c',
  'ephy-desktop-utils.c',
  'ephy-encoding-dialog.c',
  'ephy-encoding-row.c',
  'ephy-header-bar.c',
  'ephy-history-dialog.c',
  'ephy-link.c',
  'ephy-location-controller.c',
  'ephy-lockdown.c',
  'ephy-mouse-gesture-controller.c',
  'ephy-notebook.c',
  'ephy-page-row.c',
  'ephy-pages-popover.c',
  'ephy-pages-view.c',
  'ephy-search-engine-dialog.c',
  'ephy-session.c',
  'ephy-shell.c',
  'ephy-suggestion-model.c',
  'ephy-tab-header-bar.c',
  'ephy-tab-label.c',
  'ephy-window.c',
  'passwords-dialog.c',
  'popup-commands.c',
  'prefs-autofill-dialog.c',
  'prefs-autofill-utils.c',
  'prefs-dialog.c',
  'synced-tabs-dialog.c',
  'webapp-additional-urls-dialog.c',
  'window-commands.c',
  compile_schemas,
  enums
]

libephymain_deps = [
  config_h,
  ephyautofill_dep,
  ephyembed_dep,
  ephymisc_dep,
  ephysync_dep,
  ephywidgets_dep,
  gdk_dep,
  libhandy_dep,
  libnotify_dep
]

libephymain_includes = include_directories(
  '.',
  'bookmarks',
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
