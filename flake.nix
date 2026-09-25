{
  description = "sameboy-qt: a Qt 6 frontend for the SameBoy Game Boy emulator";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
    # Mirrors the third_party/SameBoy submodule pin (flake sources don't include
    # submodules). scripts/sync-sameboy.sh keeps the two in step.
    sameboy = {
      url = "github:LIJI32/SameBoy/213a12ce93d66b105a113debd9396306066a7cfc";
      flake = false;
    };
  };

  outputs = inputs@{ flake-parts, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];

      perSystem = { pkgs, lib, self', ... }:
        let
          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            pkg-config
            rgbds
            qt6.wrapQtAppsHook
          ];
          buildInputs = with pkgs; [
            qt6.qtbase
            qt6.qtmultimedia
            sdl3
          ] ++ lib.optionals pkgs.stdenv.isLinux [ qt6.qtwayland ];

          upstreamVersion =
            let
              line = lib.findFirst (lib.hasPrefix "VERSION") "VERSION := unknown"
                (lib.splitString "\n" (builtins.readFile "${inputs.sameboy}/version.mk"));
            in
            lib.trim (lib.removePrefix "VERSION :=" line);
        in
        {
          packages.default = pkgs.stdenv.mkDerivation {
            pname = "sameboy-qt";
            version = upstreamVersion;
            src = lib.cleanSourceWith {
              src = ./.;
              filter = path: type: !(lib.hasInfix "/build" path) && baseNameOf path != "result";
            };
            inherit nativeBuildInputs buildInputs;

            postUnpack = ''
              if [ ! -e "$sourceRoot/third_party/SameBoy/Core/gb.h" ]; then
                rm -rf "$sourceRoot/third_party/SameBoy"
                cp -r ${inputs.sameboy} "$sourceRoot/third_party/SameBoy"
                chmod -R u+w "$sourceRoot/third_party/SameBoy"
              fi
            '';

            cmakeFlags = [ "-DCMAKE_BUILD_TYPE=Release" ];

            doCheck = true;
            checkPhase = ''
              runHook preCheck
              QT_QPA_PLATFORM=offscreen SDL_AUDIO_DRIVER=dummy ctest --output-on-failure
              runHook postCheck
            '';

            # Desktop integration (upstream FreeDesktop icon + MIME info) must be installed.
            nativeInstallCheckInputs = [ pkgs.desktop-file-utils ];
            doInstallCheck = pkgs.stdenv.isLinux;
            installCheckPhase = ''
              runHook preInstallCheck
              desktop-file-validate $out/share/applications/sameboy-qt.desktop
              test -f $out/share/icons/hicolor/256x256/apps/sameboy-qt.png
              test -f $out/share/mime/packages/sameboy-qt.xml
              runHook postInstallCheck
            '';

            meta = {
              description = "Qt 6 frontend for the SameBoy Game Boy and Game Boy Color emulator";
              homepage = "https://sameboy.github.io";
              license = lib.licenses.mit;
              mainProgram = "sameboy-qt";
              platforms = lib.platforms.linux ++ lib.platforms.darwin;
            };
          };

          apps.default = {
            type = "app";
            program = lib.getExe self'.packages.default;
          };

          devShells.default = pkgs.mkShell {
            inherit nativeBuildInputs buildInputs;
            packages = with pkgs; [ clang-tools gdb ];
          };
        };
    };
}
