%define _lto_cflags %{nil}

%global optflags %(echo "%{optflags}" | sed 's/-g[^ ]*//g')

Name:           fcitx5-lotus
Version:        1.1.0
Release:        1
Summary:        Vietnamese input method for fcitx5
License:        GPL-3.0-or-later
URL:            https://github.com/LotusInputMethod/fcitx5-lotus
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  glibc-devel
BuildRequires:  fcitx5-devel
BuildRequires:  fcitx5-qt-devel
BuildRequires:  qt6-base-devel
BuildRequires:  libinput-devel
BuildRequires:  systemd-rpm-macros
BuildRequires:  pkgconfig(libudev)
BuildRequires:  libX11-devel

BuildRequires:  go
Requires(post): udev
BuildRequires:  sysuser-tools
BuildRequires:  libgudev-1_0-devel

%{?systemd_requires}

Requires:       fcitx5
Requires:       libinput
Requires:       hicolor-icon-theme

%description
Vietnamese input method for fcitx5

%prep
%setup -q

%build
%cmake
%cmake_build
%sysusers_generate_pre %{_prefix}/lib/sysusers.d/lotus.conf lotus

%install
%cmake_install
%find_lang %{name}

%files -f %{name}.lang
%defattr(-,root,root,-)
%dir %{_datadir}/licenses/%{name}
%license %{_datadir}/licenses/%{name}/GPL-3.0-or-later.txt
%license %{_datadir}/licenses/%{name}/LGPL-2.1-or-later.txt
%{_bindir}/fcitx5-lotus-server

%dir %{_libdir}/fcitx5
%{_libdir}/fcitx5/liblotus.so
%{_libdir}/fcitx5/qt6/libfcitx5-lotus-keymap-editor.so

%{_prefix}/lib/modules-load.d/fcitx5-lotus.conf
%{_unitdir}/fcitx5-lotus-server@.service
%{_prefix}/lib/sysusers.d/lotus.conf
%{_prefix}/lib/udev/rules.d/99-lotus.rules

%{_datadir}/fcitx5/addon/lotus.conf
%{_datadir}/fcitx5/inputmethod/lotus.conf

%dir %{_datadir}/fcitx5/lotus
%{_datadir}/fcitx5/lotus/vietnamese.cm.dict

%{_datadir}/icons/hicolor/scalable/apps/fcitx-lotus.svg
%{_datadir}/icons/hicolor/scalable/apps/org.fcitx.Fcitx5.fcitx-lotus.svg
%{_datadir}/icons/hicolor/scalable/apps/fcitx-lotus-off.svg
%{_datadir}/icons/hicolor/scalable/apps/org.fcitx.Fcitx5.fcitx-lotus-off.svg
%{_datadir}/icons/hicolor/scalable/apps/fcitx-lotus-emoji.svg
%{_datadir}/icons/hicolor/scalable/apps/org.fcitx.Fcitx5.fcitx-lotus-emoji.svg
%{_datadir}/icons/hicolor/scalable/apps/fcitx-lotus-emoji-default.svg
%{_datadir}/icons/hicolor/scalable/apps/fcitx-lotus-default.svg
%{_datadir}/icons/hicolor/scalable/apps/fcitx-lotus-off-default.svg
%{_datadir}/metainfo/org.fcitx.Fcitx5.Addon.Lotus.metainfo.xml

%clean
rm -rf %{buildroot}
rm -rf %{_builddir}/%{name}-%{version}

%pre
%sysusers_create_package lotus %{_prefix}/lib/sysusers.d/lotus.conf

%post
%systemd_post fcitx5-lotus-server@.service
if [ $1 -ge 1 ]; then
    if [ -x /usr/bin/udevadm ]; then
        /usr/sbin/modprobe uinput >/dev/null 2>&1 || :
        /usr/bin/udevadm control --reload-rules >/dev/null 2>&1 || :
        /usr/bin/udevadm trigger >/dev/null 2>&1 || :
    fi
fi

echo "--- Cấu hình Lotus ---"
echo "Hướng dẫn sau cài đặt:"
echo "1. Kích hoạt Server cho user của bạn:"
echo "   sudo systemctl enable --now fcitx5-lotus-server@\$(whoami).service"
echo ""
echo "2. Cấu hình Fcitx5:"
echo "   - Mở 'Fcitx5 Configuration', thêm bộ gõ Lotus"
echo ""
echo "3. Lưu ý cho Wayland (KDE):"
echo "   - Hãy chọn 'Fcitx 5' trong phần Virtual Keyboard của hệ thống."
echo "------------------------------------------------"

%preun
%systemd_preun fcitx5-lotus-server@.service

%postun
%systemd_postun_with_restart fcitx5-lotus-server@.service

%changelog
* Mon Feb 23 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com> - 1.1.0-1
- Add option for mode menu
- Add option for lotus icons