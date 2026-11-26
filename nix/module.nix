{
  config,
  lib,
  pkgs,
  ...
}:

let
  cfg = config.programs.appmenu-gtk-module-wayland;
  schemaDir = "${cfg.package}/share/gsettings-schemas/${cfg.package.name}";
  gtkModuleDir = "${cfg.package}/lib/gtk-3.0";
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
    environment = {
      systemPackages = [ cfg.package ];
      pathsToLink = [ "/lib/gtk-3.0" ];

      # Plasma sources /etc/xdg/plasma-workspace/env/*.sh at session startup.
      # The upstream script only exports GTK_MODULES/UBUNTU_MENUPROXY on Wayland;
      # we also append the package's GTK3 module directory to GTK_PATH and its
      # relocated GSettings schema directory to XDG_DATA_DIRS while preserving
      # existing session values.
      etc."xdg/plasma-workspace/env/appmenu-gtk-module-wayland.sh".text = ''
        . "${cfg.package}/etc/profile.d/appmenu-gtk-module-wayland.sh"
        case ":''${GTK_PATH:-}:" in
          *:"${gtkModuleDir}":*) ;;
          *) export GTK_PATH="${gtkModuleDir}''${GTK_PATH:+:$GTK_PATH}" ;;
        esac
        case ":''${XDG_DATA_DIRS:-}:" in
          *:"${schemaDir}":*) ;;
          *) export XDG_DATA_DIRS="${schemaDir}:''${XDG_DATA_DIRS:-/usr/local/share:/usr/share}" ;;
        esac
      '';
    };
  };
}
