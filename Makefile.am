## Process this file with automake to produce Makefile.in

SUBDIRS = lib embed idl src data po help doc

distcleancheck_listfiles = find . -type f -print | grep -v 'omf\.out'

distuninstallcheck_listfiles = find . -type f -print | grep -v '^\./var/scrollkeeper' | grep -v 'omf' | grep -v 'figures'

EXTRA_DIST = 			\
	intltool-merge.in	\
	intltool-update.in	\
	intltool-extract.in	\
	xmldocs.make		\
	omf.make		\
	COPYING.README		\
	MAINTAINERS		\
	ChangeLog-20030925	\
	ChangeLog-20040912

DISTCLEANFILES = 		\
	intltool-extract 	\
	intltool-merge 		\
	intltool-update

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --disable-schemas-install --with-mozilla=$(MOZILLA)
