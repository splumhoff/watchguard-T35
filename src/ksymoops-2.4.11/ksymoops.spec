Summary: Kernel oops and error message decoder
Name: ksymoops
Version: 2.4.11
Release: 1
Copyright: GPL
Group: Utilities/System
Source: ksymoops-%{version}.tar.gz
ExclusiveOS: Linux
BuildRoot: /var/tmp/ksymoops-root

%description
The Linux kernel produces error messages that contain machine specific
numbers which are meaningless for debugging.  ksymoops reads machine
specific files and the error log and converts the addresses to
meaningful symbols and offsets.

%prep
%setup
%build
CFLAGS="$RPM_OPT_FLAGS" make all

%install
rm -rf $RPM_BUILD_ROOT
make INSTALL_PREFIX=$RPM_BUILD_ROOT/usr \
     INSTALL_MANDIR=$RPM_BUILD_ROOT/%{_mandir} \
     install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%attr(755,root,root)/usr/bin/ksymoops
%attr(644,root,root)%{_mandir}/man8/ksymoops.8*
%attr(-,root,root) %doc COPYING README INSTALL Changelog
