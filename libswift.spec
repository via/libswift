Name:           libswift
Version:        1.0.0        
Release:        1%{?dist}
Summary:        C swift bindings

Group:          Other
License:        GPL
URL:            https://github.com/via/libswift.git
Source0:        %{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires: libcurl-devel
Requires:      libcurl 

%description
C client bindings for talking to OpenStack Swift


%prep
%setup -q


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



%changelog  
