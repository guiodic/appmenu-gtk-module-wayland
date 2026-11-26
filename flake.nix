{
  description = "GTK3 global menu (appmenu) module for KDE Plasma Wayland";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      packages = forAllSystems (pkgs: {
        default = pkgs.callPackage ./nix/package.nix { };
        appmenu-gtk-module-wayland = self.packages.${pkgs.stdenv.hostPlatform.system}.default;
      });

      checks = forAllSystems (pkgs: {
        default = pkgs.callPackage ./nix/package.nix { withTests = true; };
      });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          inputsFrom = [ (pkgs.callPackage ./nix/package.nix { withTests = true; }) ];
        };
      });

      overlays.default = final: _prev: {
        appmenu-gtk-module-wayland = final.callPackage ./nix/package.nix { };
      };

      nixosModules = {
        default = ./nix/module.nix;
        appmenu-gtk-module-wayland = ./nix/module.nix;
      };
    };
}
