#
# spec file for package hwinfo
#
# Copyright (c) 2025 SUSE LLC
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via https://bugs.opensuse.org/
#


Name:           hwinfo
Version:        0.0
%define lname	libhd%(echo "%version" | perl -pe 's{\\D.*}{}')
Release:        0
Summary:        Hardware Library
License:        GPL-2.0-or-later
Group:          Hardware/Other
# Until migration to github this should be correct url
URL:            https://github.com/opensuse/hwinfo
Source:         %{name}-%{version}.tar.xz
BuildRequires:  doxygen
BuildRequires:  flex
BuildRequires:  perl-XML-Parser
BuildRequires:  pkg-config
BuildRequires:  pkgconfig(udev)
BuildRequires:  pkgconfig(uuid)
%if 0%{?rhel_version} == 0
BuildRequires:  perl-XML-Writer
%endif
%ifarch %ix86 x86_64
BuildRequires:  libx86emu-devel
%endif

%description
A program that lists results from the hardware detection
library.

%package -n %lname
Summary:        Hardware detection library
Group:          System/Libraries
Provides:       libhd
Obsoletes:      libhd
Conflicts:      hwinfo < %{version}-%{release}

%description -n %lname
This library collects information about the hardware installed on a
system.

%package      devel
Summary:        Headers for the Hardware Detection Library
Group:          Development/Libraries/C and C++
Provides:       libhddev
Obsoletes:      libhddev
Requires:       %lname = %version
Requires:       perl-XML-Parser
Requires:       udev
Requires:       wireless-tools
%if 0%{?rhel_version} == 0
Requires:       perl-XML-Writer
%endif
%if 0%{?suse_version}
Requires:       libexpat-devel
%else
Requires:       expat-devel
%endif

%description devel
This library collects information about the hardware installed on a
system.

%prep
%autosetup

%build
%global _lto_cflags %{_lto_cflags} -ffat-lto-objects
  %make_build -j1 static
  # make copy of static library for installation
  cp src/libhd.a .
  %make_build -j1 clean
  %make_build -j1 LIBDIR=%{_libdir}
  %make_build -j1 doc

%install
  %make_install -j1 LIBDIR=%{_libdir}
  install -m 644 libhd.a %{buildroot}%{_libdir}
  install -d -m 755 %{buildroot}%{_mandir}/man8/
  install -d -m 755 %{buildroot}%{_mandir}/man1/
  install -m 644 doc/check_hd.1 %{buildroot}%{_mandir}/man1/
  install -m 644 doc/convert_hd.1 %{buildroot}%{_mandir}/man1/
  install -m 644 doc/getsysinfo.1 %{buildroot}%{_mandir}/man1/
  install -m 644 doc/mk_isdnhwdb.1 %{buildroot}%{_mandir}/man1/
  install -m 644 doc/hwinfo.8 %{buildroot}%{_mandir}/man8/
  mkdir -p %{buildroot}%{_tmpfilesdir}
  echo "d %{_localstatedir}/lib/hardware 0755 root root" > %{buildroot}%{_tmpfilesdir}/%{name}.conf
  echo "d %{_localstatedir}/lib/hardware/udi 0755 root root" >> %{buildroot}%{_tmpfilesdir}/%{name}.conf

%post   -n %{lname} -p %{_sbindir}/ldconfig
%postun -n %{lname} -p %{_sbindir}/ldconfig

%files
%{_sbindir}/hwinfo
%{_sbindir}/mk_isdnhwdb
%{_sbindir}/getsysinfo
%doc *.md
%doc %{_mandir}/man1/getsysinfo.1*
%doc %{_mandir}/man1/mk_isdnhwdb.1*
%doc %{_mandir}/man8/hwinfo.8*
%dir %{_datadir}/hwinfo
%{_datadir}/hwinfo/*
%{_tmpfilesdir}/%{name}.conf

%files -n %lname
%{_libdir}/libhd.so.*

%files devel
%{_sbindir}/check_hd
%{_sbindir}/convert_hd
%doc %{_mandir}/man1/convert_hd.1*
%doc %{_mandir}/man1/check_hd.1*
%{_libdir}/libhd.so
%{_libdir}/libhd.a
%{_libdir}/pkgconfig/hwinfo.pc
%{_includedir}/hd.h
%doc doc/libhd/html

%changelog
