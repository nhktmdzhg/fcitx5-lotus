{
  lib,
  stdenv,
  fetchFromGitHub,
  cmake,
  extra-cmake-modules,
  pkg-config,
  go,
  gettext,
  hicolor-icon-theme,
  fcitx5,
  libinput,
  xorg,
  libcap,
  udev,
}:
stdenv.mkDerivation rec {
  pname = "fcitx5-vmk";
  version = "0.9.4.1";

  src = fetchFromGitHub {
    owner = "nhktmdzhg";
    repo = "VMK";
    rev = "v${version}";
    fetchSubmodules = true;
    sha256 = "sha256-fnDvwcJJzfVlobAMubRDCovqJR9BjrzZsvcuHFmRG74=";
  };

  nativeBuildInputs = [
    cmake
    extra-cmake-modules
    pkg-config
    go
    gettext
    hicolor-icon-theme
  ];

  buildInputs = [
    fcitx5
    libinput
    xorg.libX11
    libcap
    udev
  ];

  preConfigure = ''
    export GOCACHE=$TMPDIR/go-cache
    export GOPATH=$TMPDIR/go

    cd bamboo
    go mod vendor
    cd ..
  '';

  cmakeFlags = [
    "-DCMAKE_INSTALL_PREFIX=${placeholder "out"}"
    "-DCMAKE_INSTALL_LIBDIR=lib"
    "-DGO_FLAGS=-mod=vendor"
  ];

  # change checking exe_path logic to make it work on NixOS since executable files on NixOS are not located in /usr/bin
  postPatch = ''
    substituteInPlace server/vmk-server.cpp \
      --replace 'std::string(exe_path) == "/usr/bin/fcitx5"' \
                '({ std::string p(exe_path); p.find("/nix/store/") == 0 && p.size() >= 11 && p.compare(p.size() - 11, 11, "/bin/fcitx5") == 0; })'
  '';

  postInstall = ''
    if [ -d "$out/lib/systemd/system" ]; then
      substituteInPlace $out/lib/systemd/system/fcitx5-vmk-server@.service \
        --replace "/usr/bin/fcitx5-vmk-server" "$out/bin/fcitx5-vmk-server"
    fi
  '';

  meta = with lib; {
    description = "Fcitx5 VMK input method";
    license = licenses.gpl3;
    platforms = platforms.linux;
  };
}
