SUBDIRS = lib embed idl src data po help doc

ACLOCAL_AMFLAGS = -I m4

distcleancheck_listfiles = find . -type f -print | grep -v 'omf\.out'

distuninstallcheck_listfiles = find . -type f -print | grep -v '/var/scrollkeeper'

EXTRA_DIST = 			\
	intltool-merge.in	\
	intltool-update.in	\
	intltool-extract.in	\
	po/LINGUAS		\
	xmldocs.make		\
	omf.make		\
	COPYING.README		\
	HACKING			\
	MAINTAINERS		\
	ChangeLog-20030925	\
	ChangeLog-20040912

DISTCLEANFILES = 		\
	intltool-extract 	\
	intltool-merge 		\
	intltool-update

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --disable-schemas-install --with-mozilla=$(MOZILLA)
