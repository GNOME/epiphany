envs = [
  'G_TEST_SRCDIR=' + meson.current_source_dir(),
  'G_TEST_BUILDDIR=' + meson.current_build_dir(),
  'GSETTINGS_SCHEMA_DIR=' + join_paths(meson.project_build_root(), 'data'),
  'GSETTINGS_BACKEND=memory',
  'PATH=' + join_paths(meson.project_build_root(), 'src') + ':' + run_command('printenv', 'PATH', check: true).stdout(),
]

test_cargs = ['-UG_DISABLE_ASSERT']

if get_option('unit_tests').enabled()
  # FIXME: The tests that need ephy-test-utils are all disabled....
  #
  # libephytestutils = static_library('ephytestutils',
  #   'ephy-test-utils.c',
  #   dependencies: ephymain_dep
  # )
  #
  # ephytestutils_dep = declare_dependency(
  #   link_with: libephytestutils
  # )

  # FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=778153
  # download_test = executable('test-ephy-download',
  #   'ephy-download-test.c',
  #   dependencies: ephymain_dep,
  #   c_args: test_cargs,
  # )
  # test('Download test',
  #      download_test,
  #      env: envs
  # )

  embed_shell_test = executable('test-ephy-embed-shell',
    'ephy-embed-shell-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs + ['-DTEST_DIR="' + meson.current_source_dir() + '"'],
  )
  test('Embed shell test',
       embed_shell_test,
       env: envs
  )

  embed_utils_test = executable('test-ephy-embed-utils',
    'ephy-embed-utils-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('Embed utils test',
       embed_utils_test,
       env: envs
  )

  encodings_test = executable('test-ephy-encodings',
    'ephy-encodings-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('Encodings test',
       encodings_test,
       env: envs
  )

  file_helpers_test = executable('test-ephy-file-helpers',
    'ephy-file-helpers-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('File helpers test',
       file_helpers_test,
       env: envs
  )

  history_test = executable('test-ephy-history',
    'ephy-history-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('History test',
       history_test,
       env: envs
  )

  location_entry_test = executable('test-location-entry',
    'ephy-location-entry-test.c',
    resources,
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('Location entry test',
       location_entry_test,
       env: envs
  )

  migration_test = executable('test-ephy-migration',
    'ephy-migration-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('Migration test',
    migration_test,
    env: envs,
    depends: ephy_profile_migrator
  )

  search_engine_manager_test = executable('test-ephy-search-engine-manager',
    'ephy-search-engine-manager-test.c',
    dependencies: ephymisc_dep,
    c_args: test_cargs,
  )
  test('Search engine manager test',
    search_engine_manager_test,
    env: envs,
  )

  # FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=707220
  # session_test = executable('test-ephy-session',
  #   'ephy-session-test.c',
  #   dependencies: ephytestutils_dep,
  #    c_args: test_cargs,
  # )
  # test('Session test',
  #      session_test,
  #      env: envs
  # )

  # FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=693369
  # FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=695703
  # FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=707217
  # shell_test = executable('test-ephy-shell',
  #   'ephy-shell-test.c',
  #   dependencies: ephytestutils_dep,
  #    c_args: test_cargs,
  # )
  # test('Shell test',
  #      shell_test,
  #      env: envs
  # )

  # FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=762753
  # snapshot_service_test = executable('test-snapshot-service',
  #   'ephy-snapshot-service-test.c',
  #   dependencies: ephymain_dep,
  #    c_args: test_cargs,
  # )
  # test('Snapshot service test',
  #      snapshot_service_test,
  #      env: envs
  # )

  sqlite_test = executable('test-ephy-sqlite',
    'ephy-sqlite-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('SQLite test',
       sqlite_test,
       env: envs
  )

  string_test = executable('test-ephy-string',
    'ephy-string-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('String test',
       string_test,
       env: envs
  )

  uri_helpers_test = executable('test-ephy-uri-helpers',
    'ephy-uri-helpers-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs,
  )
  test('URI helpers test',
    uri_helpers_test,
    env: envs
  )

  web_view_test = executable('test-ephy-web-view',
    'ephy-web-view-test.c',
    resources,
    readability_resources,
    dependencies: ephymain_dep
  )
  test('Web view test',
       web_view_test,
       env: envs
  )

  web_extension_test = executable('test-ephy-web-extension',
    'ephy-web-extension-test.c',
    dependencies: ephymain_dep,
    c_args: test_cargs
  )
  test('Web extension test',
        web_extension_test,
        env: envs
  )
endif
