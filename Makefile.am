SUBDIRS = lib embed src plugins data po help doc

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = 			\
	autogen.sh		\
	intltool-merge.in	\
	intltool-update.in	\
	intltool-extract.in	\
	gnome-doc-utils.make	\
	COPYING.README		\
	HACKING			\
	MAINTAINERS		\
	ChangeLog-20030925	\
	ChangeLog-20040912	\
	ChangeLog-20050828	\
	ChangeLog-20061106

DISTCLEANFILES = 		\
	intltool-extract 	\
	intltool-merge 		\
	intltool-update		\
	gnome-doc-utils.make

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-python			\
	--enable-zeroconf		\
	--enable-desktop-file-plugin	\
	--enable-certificate-manager	\
	--enable-gtk-doc		\
	--disable-schemas-install	\
	--disable-scrollkeeper		\
	--with-gecko=$(GECKO)

# Ignore gtk theme cache files on distcheck
distuninstallcheck_listfiles = find . -type f -print | grep -v 'icon-theme.cache'
