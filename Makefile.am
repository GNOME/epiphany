SUBDIRS = help lib embed src data po doc

if ENABLE_TESTS
SUBDIRS += tests
endif

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-debug

@CODE_COVERAGE_RULES@

# Ignore gtk theme cache files on distcheck
distuninstallcheck_listfiles = find . -type f -print | grep -v 'icon-theme.cache'

# Build ChangeLog from git history
ChangeLog:
	@if test -f $(top_srcdir)/.git/HEAD; then \
		git log --no-decorate --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

MAINTAINERCLEANFILES = \
        $(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
        $(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
        $(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL)

-include $(top_srcdir)/git.mk
