envs = [
  'G_TEST_SRCDIR=' + meson.current_source_dir(),
  'G_TEST_BUILDDIR=' + meson.current_build_dir(),
  'GSETTINGS_SCHEMA_DIR=' + join_paths(meson.build_root(), 'data'),
  'GSETTINGS_BACKEND=memory',
]

if get_option('unit_tests')
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

  completion_model_test = executable('test-ephy-completion-model',
    'ephy-completion-model-test.c',
    dependencies: ephymain_dep
  )
  test('Completion model test',
       completion_model_test,
       env: envs
  )

  # FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=778153
  # download_test = executable('test-ephy-download',
  #   'ephy-download-test.c',
  #   dependencies: ephymain_dep
  # )
  # test('Download test',
  #      download_test,
  #      env: envs
  # )

  # FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=773448
  # embed_shell_test = executable('test-ephy-embed-shell',
  #   'ephy-embed-shell-test.c',
  #   dependencies: ephymain_dep
  # )
  # test('Embed shell test',
  #      embed_shell_test,
  #      env: envs
  # )

  embed_utils_test = executable('test-ephy-embed-utils',
    'ephy-embed-utils-test.c',
    dependencies: ephymain_dep
  )
  test('Embed utils test',
       embed_utils_test,
       env: envs
  )

  encodings_test = executable('test-ephy-encodings',
    'ephy-encodings-test.c',
    dependencies: ephymain_dep
  )
  test('Encodings test',
       encodings_test,
       env: envs
  )

  # https://bugzilla.gnome.org/show_bug.cgi?id=786001
  # file_helpers_test = executable('test-ephy-file-helpers',
  #   'ephy-file-helpers-test.c',
  #   dependencies: ephymain_dep
  # )
  # test('File helpers test',
  #      file_helpers_test,
  #      env: envs
  # )

  # https://bugzilla.gnome.org/show_bug.cgi?id=796324
  #gsb_service_test = executable('test-ephy-gsb-service',
  #  'ephy-gsb-service-test.c',
  #  dependencies: ephymain_dep
  #)
  #test('GSB service test',
  #     gsb_service_test,
  #     env: envs
  #)

  history_test = executable('test-ephy-history',
    'ephy-history-test.c',
    dependencies: ephymain_dep
  )
  test('History test',
       history_test,
       env: envs
  )

  location_entry_test = executable('test-location-entry',
    'ephy-location-entry-test.c',
    dependencies: ephymain_dep
  )
  test('Location entry test',
       location_entry_test,
       env: envs
  )

  migration_test = executable('test-ephy-migration',
    'ephy-migration-test.c',
    dependencies: ephymain_dep
  )
  test('Migration test',
       migration_test,
       env: envs
  )

  # FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=707220
  # session_test = executable('test-ephy-session',
  #   'ephy-session-test.c',
  #   dependencies: ephytestutils_dep
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
  #   dependencies: ephytestutils_dep
  # )
  # test('Shell test',
  #      shell_test,
  #      env: envs
  # )

  # FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=762753
  # snapshot_service_test = executable('test-snapshot-service',
  #   'ephy-snapshot-service-test.c',
  #   dependencies: ephymain_dep
  # )
  # test('Snapshot service test',
  #      snapshot_service_test,
  #      env: envs
  # )

  sqlite_test = executable('test-ephy-sqlite',
    'ephy-sqlite-test.c',
    dependencies: ephymain_dep
  )
  test('SQLite test',
       sqlite_test,
       env: envs
  )

  string_test = executable('test-ephy-string',
    'ephy-string-test.c',
    dependencies: ephymain_dep
  )
  test('String test',
       string_test,
       env: envs
  )

  uri_helpers_test = executable('test-ephy-uri-helpers',
    'ephy-uri-helpers-test.c',
    dependencies: ephymain_dep
  )
  test('URI helpers test',
       uri_helpers_test,
       env: envs,
  )

  web_app_utils_test = executable('test-ephy-web-app-utils',
    'ephy-web-app-utils-test.c',
    dependencies: ephymain_dep
  )
  test('Web app utils test',
       web_app_utils_test,
       env: envs
  )

  # FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=780280
  # web_view_test = executable('test-ephy-web-view',
  #   'ephy-web-view-test.c',
  #   dependencies: ephymain_dep
  # )
  # test('Web view test',
  #      web_view_test,
  #      env: envs
  # )
endif
