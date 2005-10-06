SUBDIRS = lib embed idl src plugins data po help doc

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = 			\
	autogen.sh		\
	intltool-merge.in	\
	intltool-update.in	\
	intltool-extract.in	\
	po/LINGUAS		\
	gnome-doc-utils.make	\
	COPYING.README		\
	HACKING			\
	MAINTAINERS		\
	ChangeLog-20030925	\
	ChangeLog-20040912	\
	ChangeLog-20050828

DISTCLEANFILES = 		\
	intltool-extract 	\
	intltool-merge 		\
	intltool-update		\
	gnome-doc-utils.make

DISTCHECK_CONFIGURE_FLAGS = --enable-dbus --enable-python --enable-gtk-doc --disable-schemas-install --disable-scrollkeeper --with-mozilla=$(MOZILLA)
