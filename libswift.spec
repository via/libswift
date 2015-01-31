Name:           libswift
Version:        1.0.0        
Release:        1%{?dist}
Summary:        C swift bindings

Group:          Other
License:        GPL
URL:            https://github.com/via/libswift.git
Source0:        %{name}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires: libcurl-devel
Requires:      libcurl 

%description
C client bindings for talking to OpenStack Swift


%prep
%setup -q -n %{name}


%build
autoreconf -i
%configure
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%attr(0755, root, root) /usr/bin/swiftclient
/usr/include/swift.h
/usr/lib64/libswift.a
/usr/lib64/libswift.la
/usr/lib64/libswift.so
/usr/lib64/libswift.so.0
/usr/lib64/libswift.so.0.0.0
/usr/lib64/pkgconfig/swift.pc



%changelog  
