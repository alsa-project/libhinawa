%global with_tests 0
%global glib2_version 2.34.0

Name: libhinawa
Version: 2.3.0
Release: 1%{?dist}
Summary: GObject introspection library for devices connected to IEEE 1394 bus

License: LGPLv2
URL: https://github.com/alsa-project/libhinawa
Source0: https://github.com/alsa-project/libhinawa/releases/download/%{version}/libhinawa-%{version}.tar.xz

BuildRequires: meson >= 0.46.0
BuildRequires: pkgconfig(glib-2.0) >= %{glib2_version}
BuildRequires: pkgconfig(gobject-introspection-1.0) >= 1.32.1
BuildRequires: pkgconfig(gtk-doc) >= 1.18-2

%if 0%{?with_tests}
BuildRequires: python3-gobject
%endif

Requires: glib2%{?_isa} >= %{glib2_version}

%description
Hinawa is an gobject introspection library for devices connected to
IEEE 1394 bus. This library supports any types of transactions over
IEEE 1394 bus. This library also supports some functionality which
ALSA firewire stack produces.


%package devel
Summary: Development files for %{name}
Requires: %{name}%{?_isa} = %{version}-%{release}

%description devel
The %{name}-devel package contains libraries and header files for
developing applications that use %{name}.


%prep
%setup -q


%build
%meson -Dgtk_doc=true
%meson_build


%install
%meson_install

%if 0%{?with_tests}
%check
%meson_test
%endif

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%{_libdir}/libhinawa.so.*
%{_libdir}/girepository-1.0/*.typelib

%files devel
%{_includedir}/libhinawa/*
%{_libdir}/pkgconfig/*
%{_libdir}/libhinawa.so
%{_datadir}/gir-1.0/*.gir
%{_datadir}/gtk-doc/html/hinawa/*

%changelog
 * Sun Aug 29 2021 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 2.3.0
 - new upstream release.

 * Fri May 28 2021 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 2.2.1
 - new upstream release.

 * Thu Aug 24 2020 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 2.2.0
 - new upstream release.

 * Thu Aug 18 2020 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 2.1.0
 - new upstream release.

 * Thu May 20 2020 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 2.0.0
 - new upstream release.

 * Thu May 11 2020 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 1.4.5
 - new upstream release.

 * Thu Jan 20 2020 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 1.4.4
 - new upstream release.

 * Thu Jan 16 2020 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 1.4.3
 - new upstream release.

 * Thu Dec 5 2019 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 1.4.2
 - new upstream release.

 * Mon Dec 2 2019 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 1.4.1
 - new upstream release.

 * Sat Oct 12 2019 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 1.4.0
 - new upstream release.

 * Tue May 7 2019 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 1.3.1
 - new upstream release.

 * Mon Apr 22 2019 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 1.3.0
 - new upstream release.

 * Sun Mar 21 2019 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 1.2.0
 - new upstream release.

 * Sun Mar 5 2019 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 1.1.2
 - new upstream release with bug fixes.

 * Sun Feb 25 2019 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 1.1.1
 - new upstream release with bug fixes.

 * Sun Dec 30 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 1.1.0
 - new upstream release.

 * Sat Sep 8 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 1.0.1
 - new upstream release.

 * Tue Jun 20 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 1.0.0
 - new upstream release.

 * Tue Apr 10 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 0.9.0
 - new upstream release.

 * Sun Sep 24 2017 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 0.8.2-1
 - new upstream release with minor improvements.

 * Fri May 7 2017 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 0.8.1-1
 - new upstream bugfix release.

 * Sat Apr 22 2017 Takashi Sakamoto <o-takashi@sakamocchi.jp> - 0.8.0-1
 - new upstream release.

 * Fri Feb  5 2016 HAYASHI Kentaro <hayashi@clear-code.com> - 0.7.0-1
 - new upstream release.

 * Tue Mar  3 2015 Yoshihiro Okada - 0.5.0-1
 - new upstream release.
