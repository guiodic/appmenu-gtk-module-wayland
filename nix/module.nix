{
  config,
  lib,
  pkgs,
  ...
}:

let
  cfg = config.programs.appmenu-gtk-module-wayland;
in
{
  options.programs.appmenu-gtk-module-wayland = {
    enable = lib.mkEnableOption "GTK3 global menu (appmenu) module for KDE Plasma Wayland";

    package = lib.mkOption {
      type = lib.types.package;
      default = pkgs.callPackage ./package.nix { };
      defaultText = lib.literalExpression "pkgs.callPackage ./package.nix { }";
      description = "The appmenu-gtk-module-wayland package to use.";
    };
  };

  config = lib.mkIf cfg.enable {
    environment.systemPackages = [ cfg.package ];

    # The system profile already exposes lib/gtk-3.0 on GTK_PATH (pathsToLink),
    # so the module in $out/lib/gtk-3.0/modules is discovered by name.
    # Plasma sources /etc/xdg/plasma-workspace/env/*.sh at session startup;
    # the upstream script only exports GTK_MODULES/UBUNTU_MENUPROXY on Wayland.
    environment.etc."xdg/plasma-workspace/env/appmenu-gtk-module-wayland.sh".source =
      "${cfg.package}/etc/profile.d/appmenu-gtk-module-wayland.sh";

    # Expose org.appmenu.gtk-module.wayland to gsettings and GTK apps without
    # shadowing other packages' schemas (GLib relocates schemas into
    # share/gsettings-schemas/<name>/glib-2.0/schemas).
    environment.sessionVariables.XDG_DATA_DIRS = [
      "${cfg.package}/share/gsettings-schemas/${cfg.package.name}"
    ];
  };
}
