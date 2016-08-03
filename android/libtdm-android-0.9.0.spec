Name:           libtdm-android
Version:        0.9.0
Release:        1
License:        MIT
Summary:        Tizen Display Manager Android Back-End Library
Group:          System/Libraries
Source0:        %{name}-%{version}.tar.gz

%description
Back-End library of Tizen Display Manager Android : libtdm-mgr Android library

%prep
# our %{name}-%{version}.tar.gz archive hasn't top-level directory,
# so we create it
%setup -q -c %{name}-%{version}

%build
PKG_CONFIG_PATH=/usr/local/lib/pkgconfig \
./autogen.sh --build=x86_64-unknown-linux-gnu \
	     --host=arm-linux-androideabi \
	     --disable-static \
             CFLAGS="${CFLAGS} -Wall -Werror" \
             LDFLAGS="${LDFLAGS} -Wl,--hash-style=both -Wl,--as-needed"
	     
make %{?_smp_mflags}

%install
rm -rf %{buildroot}

%make_install
cd %{buildroot}/usr/local/lib/tdm/ && ln -s libtdm-android.so libtdm-default.so

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
/usr/local/*
