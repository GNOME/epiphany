SUBDIRS = lib embed src data po help doc

if ENABLE_TESTS
SUBDIRS += tests
endif

ACLOCAL_AMFLAGS = -I m4

NULL =

EXTRA_DIST = 			\
	autogen.sh		\
	gnome-doc-utils.make	\
	COPYING.README		\
	HACKING			\
	MAINTAINERS		\
	ChangeLog		\
	ChangeLog-20030925	\
	ChangeLog-20040912	\
	ChangeLog-20050828	\
	ChangeLog-20061106	\
	ChangeLog-20070912	\
	po/ChangeLog.pre-2-23	\
	help/ChangeLog.pre-2-23	\
	$(NULL)

DISTCLEANFILES = 		\
	gnome-doc-utils.make	\
	$(NULL)

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-zeroconf		\
	--enable-gtk-doc		\
	--disable-schemas-install	\
	--disable-scrollkeeper

# Ignore gtk theme cache files on distcheck
distuninstallcheck_listfiles = find . -type f -print | grep -v 'icon-theme.cache'

# Build ChangeLog from git history
ChangeLog:
	@if test -f $(top_srcdir)/.git/HEAD; then \
		git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog
