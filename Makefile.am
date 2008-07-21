SUBDIRS = lib embed src plugins data po help doc

#if WITH_XULRUNNER_ENGINE
#SUBDIRS += xulapp
#endif

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

# Build ChangeLog from SVN history (r7394 was the first ChangeLog-less checkin).
# Only build this when in an svn checkout.

ChangeLog:
	@if test -f $(top_srcdir)/.git/HEAD; then \
		git log --stat > $@; \
	elif test -f $(top_srcdir)/.svn/entries; then \
		svn log -v --xml -r HEAD:7394 $(SVN_ROOT)/$(SVN_MODULE)/$(SVN_BRANCH) | \
			xsltproc --stringparam strip-prefix "$(SVN_MODULE)/$(SVN_BRANCH)" \
				 --stringparam include-rev "yes" $(top_srcdir)/svn2cl.xsl - > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog
