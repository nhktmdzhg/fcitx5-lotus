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
  libx11,
  libcap,
  udev,
}:
stdenv.mkDerivation rec {
  pname = "fcitx5-lotus";
  version = "1.0.0";

  src = fetchFromGitHub {
    owner = "LotusInputMethod";
    repo = "fcitx5-lotus";
    rev = "v${version}";
    fetchSubmodules = true;
    sha256 = "sha256-n8kG/mYYxCSZCRCtBq1QR9g2zueZ57l5r9tp/dUg7v0=";
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
    libx11
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
    substituteInPlace src/lotus-monitor.cpp \
      --replace-fail 'strcmp(exe_path, "/usr/bin/fcitx5-lotus-server") == 0' \
                '(strncmp(exe_path, "/nix/store/", 22) == 0 && strlen(exe_path) >= 22 && strcmp(exe_path + strlen(exe_path) - 22, "/bin/fcitx5-lotus-server") == 0)'
    substituteInPlace server/lotus-server.cpp \
      --replace-fail 'strcmp(exe_path, "/usr/bin/fcitx5") == 0' \
                '(strncmp(exe_path, "/nix/store/", 11) == 0 && strlen(exe_path) >= 11 && strcmp(exe_path + strlen(exe_path) - 11, "/bin/fcitx5") == 0)'
  '';

  postInstall = ''
    if [ -d "$out/lib/systemd/system" ]; then
      substituteInPlace $out/lib/systemd/system/fcitx5-lotus-server@.service \
        --replace-fail "/usr/bin/fcitx5-lotus-server" "$out/bin/fcitx5-lotus-server"
    fi
  '';

  meta = with lib; {
    description = "Fcitx5 Lotus input method";
    license = licenses.gpl3;
    platforms = platforms.linux;
  };
}
