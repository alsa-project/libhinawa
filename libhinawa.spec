Name:			libhinawa
Version:		1.1.1
Release:		1%{?dist}
Summary:		GObject introspection library for devices connected to IEEE 1394 bus

License:		LGPLv2
URL:			https://github.com/takaswie/libhinawa
Source:			%{name}-%{version}.tar.xz

BuildRequires:  meson >= 0.32.0
BuildRequires:  gtk-doc >= 1.18-2
BuildRequires:  gobject-introspection >= 1.32.1, gobject-introspection-devel >= 1.32.1
BuildRequires:  python3-gobject


%description
Hinawa is an gobject introspection library for devices connected to
IEEE 1394 bus. This library supports any types of transactions over
IEEE 1394 bus.  This library also supports some functionality which
ALSA firewire stack produces.

%package		devel
Summary:		Development files for %{name}
Requires:		%{name}%{?_isa} = %{version}-%{release}

%description	devel
The %{name}-devel package contains libraries and header files for
developing applications that use %{name}.


%prep
%setup -q


%build
%meson
%meson_build


%install
%meson_install


%check
%meson_test


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%{_libdir}/libhinawa.so.*
%{_libdir}/girepository-1.0/*

%files devel
%{_includedir}/libhinawa/*
%{_libdir}/pkgconfig/*
%{_libdir}/libhinawa.so
%{_datadir}/gir-1.0/*
%{_datadir}/gtk-doc/html/hinawa/*


%changelog
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
