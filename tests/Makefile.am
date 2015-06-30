SUBDIRS = data

noinst_PROGRAMS = \
	test-ephy-bookmarks \
	test-ephy-completion-model \
	test-ephy-download \
	test-ephy-embed-shell \
	test-ephy-embed-utils \
	test-ephy-encodings \
	test-ephy-file-helpers \
	test-ephy-history \
	test-ephy-location-entry \
	test-ephy-migration \
	test-ephy-snapshot-service \
	test-ephy-sqlite \
	test-ephy-string \
	test-ephy-uri-helpers \
	test-ephy-web-app-utils \
	test-ephy-web-view \
	$(NULL)

# Mostly copied from Makefile.decl in glib
GTESTER = gtester
GTESTER_REPORT = gtester-report

# initialize variables for unconditional += appending
TEST_PROGS = ${noinst_PROGRAMS}

### testing rules

# test: run all tests in cwd and subdirs
test: test-nonrecursive
	@for subdir in $(SUBDIRS) . ; do \
		test "$$subdir" = "." -o "$$subdir" = "po" || \
		( cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) $@ ) || exit $? ; \
	done

# test-nonrecursive: run tests only in cwd
test-nonrecursive: ${TEST_PROGS}
	@test -z "${TEST_PROGS}" || G_DEBUG=gc-friendly MALLOC_CHECK_=2 MALLOC_PERTURB_=$$(($${RANDOM:-256} % 256)) ${GTESTER} --verbose ${TEST_PROGS}

# test-report: run tests in subdirs and generate report
# perf-report: run tests in subdirs with -m perf and generate report
# full-report: like test-report: with -m perf and -m slow
test-report perf-report full-report: ${TEST_PROGS}
	@test -z "${TEST_PROGS}" || { \
	  case $@ in \
	  test-report) test_options="-k";; \
	  perf-report) test_options="-k -m=perf";; \
	  full-report) test_options="-k -m=perf -m=slow";; \
	  esac ; \
	  if test -z "$$GTESTER_LOGDIR" ; then	\
	    ${GTESTER} --verbose $$test_options -o test-report.xml ${TEST_PROGS} ; \
	  elif test -n "${TEST_PROGS}" ; then \
	    ${GTESTER} --verbose $$test_options -o `mktemp "$$GTESTER_LOGDIR/log-XXXXXX"` ${TEST_PROGS} ; \
	  fi ; \
	}
	@ ignore_logdir=true ; \
	  if test -z "$$GTESTER_LOGDIR" ; then \
	    GTESTER_LOGDIR=`mktemp -d "\`pwd\`/.testlogs-XXXXXX"`; export GTESTER_LOGDIR ; \
	    ignore_logdir=false ; \
	  fi ; \
	  if test -d "$(top_srcdir)/.git" ; then \
	    REVISION=`git describe` ; \
	  else \
	    REVISION=$(VERSION) ; \
	  fi ; \
	  for subdir in $(SUBDIRS) . ; do \
	    test "$$subdir" = "." -o "$$subdir" = "po" || \
	    ( cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) $@ ) || exit $? ; \
	  done ; \
	  $$ignore_logdir || { \
	    echo '<?xml version="1.0"?>'	      > $@.xml ; \
	    echo '<report-collection>'		     >> $@.xml ; \
	    echo '<info>'			     >> $@.xml ; \
	    echo '  <package>$(PACKAGE)</package>'   >> $@.xml ; \
	    echo '  <version>$(VERSION)</version>'   >> $@.xml ; \
	    echo "  <revision>$$REVISION</revision>" >> $@.xml ; \
	    echo '</info>'			     >> $@.xml ; \
	    for lf in `ls -L "$$GTESTER_LOGDIR"/.` ; do \
	      sed '1,1s/^<?xml\b[^>?]*?>//' <"$$GTESTER_LOGDIR"/"$$lf" >> $@.xml ; \
	    done ; \
	    echo >> $@.xml ; \
	    echo '</report-collection>' >> $@.xml ; \
	    rm -rf "$$GTESTER_LOGDIR"/ ; \
	    ${GTESTER_REPORT} --version 2>/dev/null 1>&2 ; test "$$?" != 0 || ${GTESTER_REPORT} $@.xml >$@.html ; \
	  }
.PHONY: test test-report perf-report full-report test-nonrecursive

# run tests in cwd as part of make check
check-local: test-nonrecursive

AM_CPPFLAGS = \
	-I$(top_srcdir)/embed    \
	-I$(top_srcdir)/lib      \
	-I$(top_srcdir)/lib/history \
	-I$(top_srcdir)/lib/widgets \
	-I$(top_builddir)/lib/widgets \
	-I$(top_srcdir)/src      \
	-I$(top_srcdir)/src/bookmarks

AM_CFLAGS = \
	-DTEST_DIR=\"$(srcdir)\"\
	$(DEPENDENCIES_CFLAGS)  \
	$(CODE_COVERAGE_CFLAGS) \
	$(WARN_CFLAGS)

AM_LDFLAGS = $(WARN_LDFLAGS)

LDADD = \
	$(top_builddir)/src/libephymain.la \
	$(top_builddir)/src/bookmarks/libephybookmarks.la \
	$(top_builddir)/embed/libephyembed.la \
	$(top_builddir)/lib/widgets/libephywidgets.la \
	$(top_builddir)/lib/history/libephyhistory.la \
	$(top_builddir)/lib/libephymisc.la \
	$(top_builddir)/lib/egg/libegg.la \
	$(DBUS_LIBS) \
	$(CODE_COVERAGE_LDFLAGS) \
	$(DEPENDENCIES_LIBS) 

test_ephy_bookmarks_SOURCES = \
	ephy-bookmarks-test.c

test_ephy_completion_model_SOURCES = \
	ephy-completion-model-test.c

test_ephy_download_SOURCES = \
	ephy-download-test.c

test_ephy_embed_shell_SOURCES = \
	ephy-embed-shell-test.c

test_ephy_embed_utils_SOURCES = \
	ephy-embed-utils-test.c

test_ephy_encodings_SOURCES = \
	ephy-encodings-test.c

test_ephy_file_helpers_SOURCES = \
	ephy-file-helpers-test.c
test_ephy_file_helpers_CPPFLAGS = \
	-DTOP_SRC_DIR=\"$(abs_top_srcdir)\" \
	$(AM_CPPFLAGS)

test_ephy_history_SOURCES = \
	ephy-history-test.c

test_ephy_location_entry_SOURCES = \
	ephy-location-entry-test.c

test_ephy_migration_SOURCES = \
	ephy-migration-test.c

# test_ephy_session_SOURCES = \
# 	ephy-session-test.c \
# 	ephy-test-utils.c \
# 	ephy-test-utils.h \
# 	$(top_builddir)/src/epiphany-resources.c \
# 	$(top_builddir)/src/epiphany-resources.h

# test_ephy_shell_SOURCES = \
# 	$(top_builddir)/src/epiphany-resources.c \
# 	$(top_builddir)/src/epiphany-resources.h \
# 	ephy-shell-test.c \
# 	ephy-test-utils.c \
# 	ephy-test-utils.h

test_ephy_snapshot_service_SOURCES = \
	ephy-snapshot-service-test.c

test_ephy_sqlite_SOURCES = \
	ephy-sqlite-test.c

test_ephy_string_SOURCES = \
	ephy-string-test.c

test_ephy_uri_helpers_SOURCES = \
	ephy-uri-helpers-test.c

test_ephy_web_app_utils_SOURCES = \
	ephy-web-app-utils-test.c

test_ephy_web_view_SOURCES = \
	$(top_builddir)/src/epiphany-resources.c \
	ephy-web-view-test.c

EXTRA_DIST = \
	data/test.html \
	applications/epiphany.desktop \
	applications/defaults.list

-include $(top_srcdir)/git.mk
