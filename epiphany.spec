Summary: GNOME web browser based on the Mozilla rendering engine
Name: epiphany
Version: 0.5.0
Release: 2
License: GPL
Group: Applications/Internet
URL: http://epiphany.mozdev.org
Source0: http://downloads.mozdev.org/epiphany/%{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
Prereq: scrollkeeper, GConf2
Requires: mozilla = 35:1.3-0_rh8_gtk2
BuildRequires: mozilla-devel
BuildRequires: gtk2-devel
BuildRequires: libbonoboui-devel >= 2.1.1
BuildRequires: scrollkeeper >= 0.1.4
BuildRequires: libxml2-devel, libgnomeui-devel, libglade2-devel
BuildRequires: gnome-vfs2-devel, GConf2-devel, ORBit2-devel

%description
epiphany is a simple GNOME web browser based on the Mozilla rendering
engine

%prep
%setup -q

%build
%configure --enable-nautilus-view=no
make

%install
rm -rf $RPM_BUILD_ROOT
export GCONF_DISABLE_MAKEFILE_SCHEMA_INSTALL=1
%makeinstall
unset GCONF_DISABLE_MAKEFILE_SCHEMA_INSTALL

%find_lang %{name}-2.0

rm -rf $RPM_BUILD_ROOT/var/scrollkeeper/


%post
export GCONF_CONFIG_SOURCE=`gconftool-2 --get-default-source`
SCHEMAS="epiphany.schemas"
for S in $SCHEMAS; do
  gconftool-2 --makefile-install-rule /etc/gconf/schemas/$S > /dev/null
done
scrollkeeper-update

%postun
scrollkeper-update

%clean
rm -rf $RPM_BUILD_ROOT

%files -f %{name}-2.0.lang
%defattr(-,root,root,-)
%doc
%{_sysconfdir}/gconf/schemas/epiphany.schemas
%{_bindir}/epiphany
%{_bindir}/epiphany-bin
%{_libdir}/bonobo/servers/*
%{_datadir}/applications/*.desktop
%{_datadir}/epiphany
%{_datadir}/gnome/help/epiphany
%{_datadir}/omf/epiphany
%{_datadir}/pixmaps/*png



%changelog
* Mon Apr 14 2003 Jeremy Katz <katzj@redhat.com> 0.5.0-2
- add some buildrequires, prereq GConf2
- disable building nautilus view

* Sun Apr 13 2003 Jeremy Katz <katzj@redhat.com> 
- Initial build.


