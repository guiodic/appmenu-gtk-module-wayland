{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
  };

  outputs = inputs:
    inputs.flake-parts.lib.mkFlake {inherit inputs;} {
      systems = ["x86_64-linux"];
      perSystem = {
        self',
        system,
        lib,
        config,
        pkgs,
        ...
      }: {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "appmenu-gtk-module-wayland";
          version = "0.0.1";
          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
            glib.dev
            python3
            dbus
            xvfb-run
          ];

          buildInputs = with pkgs; [
            gtk3
            libdbusmenu-gtk3
            libdbusmenu
            wayland
            libx11.dev
            libxdmcp.dev
          ];

          cmakeFlags = [
            (lib.cmakeBool "BUILD_TESTS" true)
            "-DPROFILE_INSTALL_DIR=etc/profile.d"
          ];

          postInstall = ''
            mkdir -p $out/bin
            cp unity-gtk-menu-tester $out/bin/
          '';
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [self'.packages.default];
        };
      };
    };
}
