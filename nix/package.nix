{
  lib,
  stdenv,
  cmake,
  pkg-config,
  glib,
  gtk3,
  libdbusmenu,
  libdbusmenu-gtk3,
  libX11,
  wayland,
  withTests ? false,
  python3,
  dbus,
  xvfb-run,
}:

stdenv.mkDerivation {
  pname = "appmenu-gtk-module-wayland";
  version = "0-unstable-2026-11-05";

  src = lib.cleanSource ./..;

  strictDeps = true;

  nativeBuildInputs = [
    cmake
    pkg-config
    glib.dev
  ]
  ++ lib.optionals withTests [
    python3
    dbus
    xvfb-run
  ];

  buildInputs = [
    glib
    gtk3
    libdbusmenu
    libdbusmenu-gtk3
    libX11
    wayland
  ];

  cmakeFlags = [
    (lib.cmakeFeature "PROFILE_INSTALL_DIR" "${placeholder "out"}/etc/profile.d")
    (lib.cmakeBool "BUILD_TESTS" withTests)
  ];

  # The Nix build sandbox has neither /bin/bash nor /etc/dbus-1/session.conf.
  postPatch = lib.optionalString withTests ''
    substituteInPlace tests/test-session-environment.py \
      --replace-fail "'/bin/bash'" "'${stdenv.shell}'"
    substituteInPlace tests/run-mnemonics.py \
      --replace-fail "args.dbus_run_session, '--'" "args.dbus_run_session, '--config-file=${dbus}/share/dbus-1/session.conf', '--'"
  '';

  doCheck = withTests;
  checkPhase = ''
    runHook preCheck
    export HOME="$TMPDIR"
    export XDG_CACHE_HOME="$TMPDIR/cache"
    ctest --output-on-failure -j1
    runHook postCheck
  '';

  meta = {
    description = "GTK3 module that exports application menus on Plasma Wayland";
    homepage = "https://github.com/guiodic/appmenu-gtk-module-wayland";
    license = lib.licenses.lgpl3Plus;
    platforms = lib.platforms.linux;
  };
}
