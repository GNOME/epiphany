## Process this file with automake to produce Makefile.in

SUBDIRS = lib embed src data po help plugins

distcleancheck_listfiles = find . -type f -print | grep -v 'omf\.out'

distuninstallcheck_listfiles = find . -type f -print | grep -v '^\./var/scrollkeeper' | grep -v 'omf' | grep -v 'figures'

EXTRA_DIST = 			\
	intltool-merge.in	\
	intltool-update.in	\
	intltool-extract.in	\
	xmldocs.make		\
	omf.make		\
	COPYING.README

DISTCLEANFILES = 		\
	intltool-extract 	\
	intltool-merge 		\
	intltool-update
