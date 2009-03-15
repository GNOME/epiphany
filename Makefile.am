SUBDIRS = lib embed src plugins data po help doc

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
	--enable-python			\
	--disable-zeroconf		\
	--enable-desktop-file-plugin	\
	--enable-gtk-doc		\
	--disable-schemas-install	\
	--disable-scrollkeeper		\
	--with-gecko=$(GECKO)

# Ignore gtk theme cache files on distcheck
distuninstallcheck_listfiles = find . -type f -print | grep -v 'icon-theme.cache'

ChangeLog: $(top_srcdir)/.git/HEAD
	git log --stat > $@

dist: ChangeLog

.PHONY: ChangeLog
