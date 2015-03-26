%{!?_httpd_mmn: %{expand: %%global _httpd_mmn %%(cat %{_includedir}/httpd/.mmn || echo missing-httpd-devel)}}
%{!?_httpd_apxs: %{expand: %%global _httpd_apxs %%{_sbindir}/apxs}}

Name:           mod_statsd
Version:        1.0.5
Release:        1%{?dist}
Summary:        Apache module to send statistics to Statsd
Group:          System Environment/Daemons
License:        MIT
URL:            https://github.com/jib/mod_statsd
Source0:        %{name}.tar.gz
BuildRoot:      %{name}-%{version}-%{release}
BuildRequires:  httpd-devel >= 2.0.0
Requires:       httpd-mmn = %{_httpd_mmn}

%description
This module enables the sending of Statsd statistics directly from Apache,
without the need for a CustomLog processor.
It will send one counter and one timer per request received.

%prep
%setup -n src

%build
%{_httpd_apxs} -Wc,"%{optflags}" -c mod_statsd.c

%install
mkdir -p %{buildroot}%{_sysconfdir}/httpd/conf.d/
mkdir -p %{buildroot}%{_libdir}/httpd/modules/
install -Dpm 755 .libs/mod_statsd.so \
    %{buildroot}%{_libdir}/httpd/modules/mod_statsd.so
install -Dpm 644 mod_statsd.conf \
    %{buildroot}%{_sysconfdir}/httpd/conf.d/mod_statsd.conf

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc DOCUMENTATION LICENSE
%{_libdir}/httpd/modules/mod_statsd.so
%config(noreplace) %{_sysconfdir}/httpd/conf.d/mod_statsd.conf

###############################################################################
#                                  CHANGELOG                                  #
###############################################################################

%changelog
* Wed Mar 25 2015 Andy "Bob" Brockhurst <andy.brockhurst@b3cft.com> - 1.0.5-2
- removed patch as no longer required
- updated spec to use rpm macros

* Wed Oct 8 2014 Matthew Hollick <matthew@mayan-it.co.uk> - 1.0.5-1
- New package
- Copied spec file from mod_qos by Christof Damian.
- Source from https://github.com/jib/mod_statsd
