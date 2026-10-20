{
  description = "MONICA (C++) - build and development environment";

  inputs.nixpkgs.url = "nixpkgs";

  # mas_cpp_misc (and its nested schema submodule) are git submodules; without
  # this a flake's source copy has them as empty directories.
  inputs.self.submodules = true;

  outputs =
    { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};

      # The pre-generated schema code (mas_cpp_misc/mas_capnproto_schemas/gen/c++)
      # refuses to compile against any Cap'n Proto but the one that generated it
      # (`#elif CAPNP_VERSION != 1005000`). 1.5.0 is also what the vcpkg baseline
      # in vcpkg_tag.txt resolves to. While nixpkgs ships exactly that, its
      # package is used as is (prebuilt in the binary cache); once nixpkgs moves
      # on, the same recipe is rebuilt from the pinned source.
      capnprotoVersion = "1.5.0";
      capnproto =
        if pkgs.capnproto.version == capnprotoVersion then
          pkgs.capnproto
        else
          pkgs.capnproto.overrideAttrs (old: {
            version = capnprotoVersion;
            src = pkgs.fetchFromGitHub {
              owner = "capnproto";
              repo = "capnproto";
              rev = "v${capnprotoVersion}";
              hash = "sha256-2J3FYwPAtbahHI1y1KMqU8Gn2YlKyIW8kZIJz2Ja31w=";
            };
            # nixpkgs' patches are for its own version and may not apply to
            # another; the one it has today only matters on musl.
            patches = [ ];
          });

      # vcpkg's `unofficial-sodium::sodium` target, rebuilt from libsodium's
      # pkg-config file - see cmake/compat/unofficial-sodium.
      compat = ./cmake/compat;

      # Everything the C++ Dockerfile copies out of its build, plus
      # storage-service. CMake has no install() rules, hence the list.
      executables = [
        "monica-run"
        "monica-zmq-proxy"
        "monica-zmq-server"
        "monica-capnp-server"
        "monica-capnp-proxy"
        "monica-capnp-fbp-component"
        "daily-monica-fbp-component"
        "common/channel"
        "common/gateway-service"
        "common/host-port-resolver-service"
        "common/storage-service"
      ];

      monica = pkgs.stdenv.mkDerivation {
        pname = "monica";
        version = self.shortRev or self.dirtyShortRev or "dev";
        # monica-parameters is runtime data only; leaving it out keeps a
        # parameter change from rebuilding MONICA
        src = pkgs.lib.fileset.toSource {
          root = ./.;
          fileset = pkgs.lib.fileset.difference ./. ./monica-parameters;
        };
        nativeBuildInputs = [
          pkgs.cmake
          pkgs.ninja
          pkgs.pkg-config
        ];
        buildInputs = [
          capnproto
          pkgs.zeromq
          pkgs.libsodium
          pkgs.zlib
        ];
        cmakeFlags = [
          (pkgs.lib.cmakeFeature "CMAKE_BUILD_TYPE" "Release")
          (pkgs.lib.cmakeFeature "CMAKE_PREFIX_PATH" "${compat}")
        ];
        installPhase = ''
          runHook preInstall
          for e in ${pkgs.lib.concatStringsSep " " executables}; do
            install -Dm755 "$e" -t $out/bin
          done
          runHook postInstall
        '';

        meta = {
          description = "MONICA - Model for Nitrogen and Carbon in Agro-ecosystems";
          homepage = "https://github.com/zalf-rpm/monica";
          license = pkgs.lib.licenses.mpl20;
          platforms = pkgs.lib.platforms.linux;
          # what `nix run` starts. From GitHub that needs the git+https form:
          # github: URLs fetch a tarball, which never contains submodules.
          mainProgram = "monica-run";
        };
      };
    in
    {
      packages.${system} = {
        inherit monica capnproto;
        default = monica;
      };

      # `nix develop`, then the usual: cmake -B _build -G Ninja && cmake --build _build
      # Same dependencies as the package, plus tooling; no vcpkg needed.
      devShells.${system}.default = pkgs.mkShell {
        inputsFrom = [ monica ];
        packages = with pkgs; [
          clang-tools # clangd, clang-format
          gdb
        ];
        shellHook = ''
          export CMAKE_PREFIX_PATH="${compat}''${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
        '';
      };
    };
}
