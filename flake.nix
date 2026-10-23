{
  description = "MONICA (Odin) mit C++-Toolchain";

  # Odin entwickelt sich schnell -> bei Bedarf auf "nixpkgs-unstable" umstellen
  inputs.nixpkgs.url = "nixpkgs";

  # The Cap'n Proto shim (odin/support/capnp) and its schemas are git
  # submodules; without this a flake's source copy has them as empty dirs.
  inputs.self.submodules = true;

  outputs =
    { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      lib = pkgs.lib;

      # Shared with odin/tools/odinw.py (the pixi build), so both pin the same
      # Odin compiler, zeromq and Cap'n Proto.
      versions = builtins.fromJSON (builtins.readFile ./odin/versions.json);

      # The exact release odinw.py bootstraps (same asset, same SHA256), not
      # nixpkgs' odin, which is a different nightly. The Linux release binary
      # is statically linked, so it needs no patchelf.
      odin = pkgs.stdenvNoCC.mkDerivation {
        pname = "odin";
        version = versions.odin.tag;
        src = pkgs.fetchurl {
          url = "https://github.com/odin-lang/Odin/releases/download/${versions.odin.tag}/odin-linux-amd64-${versions.odin.tag}.tar.gz";
          sha256 = versions.odin.sha256.linux;
        };
        nativeBuildInputs = [ pkgs.makeWrapper ];
        installPhase = ''
          mkdir -p $out/share $out/bin
          cp -r . $out/share/odin
          makeWrapper $out/share/odin/odin $out/bin/odin --set-default ODIN_ROOT $out/share/odin
        '';
      };

      # The version the vcpkg baseline (pixi build) resolves to. When nixpkgs
      # ships exactly that, use its package as is (prebuilt in the binary
      # cache); otherwise rebuild nixpkgs' recipe from the pinned source.
      capnproto =
        if pkgs.capnproto.version == versions.capnproto.version then
          pkgs.capnproto
        else
          pkgs.capnproto.overrideAttrs (old: {
            version = versions.capnproto.version;
            src = pkgs.fetchFromGitHub {
              owner = "capnproto";
              repo = "capnproto";
              rev = "v${versions.capnproto.version}";
              hash = versions.capnproto.nix_src_hash;
            };
            # nixpkgs' patches are for its own version and may not apply to
            # another; the one it has today only matters on musl.
            patches = [ ];
          });

      zeromq =
        assert lib.assertMsg (pkgs.zeromq.version == versions.zeromq)
          "nixpkgs zeromq is ${pkgs.zeromq.version}, odin/versions.json pins ${versions.zeromq}";
        pkgs.zeromq;

      # Everything below builds with clang, like Cap'n Proto itself (see the
      # nixpkgs recipe: gcc miscompiles its coroutines) and like Odin links.
      capnp-shim = pkgs.clangStdenv.mkDerivation {
        pname = "mas-capnp-dynamic-shim";
        version = versions.capnproto.version;
        src = ./odin/support/capnp/shim_dynamic;
        nativeBuildInputs = [
          pkgs.cmake
          pkgs.ninja
        ];
        buildInputs = [ capnproto ];
        # The CMake project has no install() rules.
        installPhase = ''
          install -Dm755 libmas_capnp_dynamic_shim.so -t $out/lib
        '';
      };

      # CORE-MATH, which every transcendental call in the port goes through
      # (odin/CONVENTIONS.md §1). mas_cpp_misc's own CMakeLists.txt, so the
      # flags (-ffp-contract=off, FMA) are exactly the C++ build's.
      cr-math = pkgs.clangStdenv.mkDerivation {
        pname = "cr-math";
        version = "mas_cpp_misc";
        src = ./mas_cpp_misc/cr_math;
        nativeBuildInputs = [
          pkgs.cmake
          pkgs.ninja
        ];
        cmakeFlags = [ (lib.cmakeBool "CMAKE_POSITION_INDEPENDENT_CODE" true) ];
        # The CMake project has no install() rules.
        installPhase = ''
          install -Dm644 libcr_math_lib.a -t $out/lib
        '';
      };

      # --- TEMPORARY: the converted C++ in src/ -------------------------------
      # Built only so CI can check that it stays in sync with the original C++
      # (master) and the Odin port, while it is still needed as a reference
      # (e.g. for the serde transformation). Remove this block, cmake/compat/,
      # its entry in `packages` below and the `cpp-src` job in
      # .github/workflows/odin.yml once the move to Odin is complete.
      monica-cpp-src = pkgs.stdenv.mkDerivation {
        pname = "monica-cpp-src";
        version = self.shortRev or self.dirtyShortRev or "dev";
        # only what the C++ build reads, so Odin-only changes don't rebuild it
        src = lib.fileset.toSource {
          root = ./.;
          fileset = lib.fileset.unions [
            ./CMakeLists.txt
            ./src
            ./mas_cpp_misc
          ];
        };
        nativeBuildInputs = [
          pkgs.cmake
          pkgs.ninja
          pkgs.pkg-config
        ];
        buildInputs = [
          capnproto
          zeromq
          pkgs.libsodium
          pkgs.zlib
        ];
        cmakeFlags = [
          (lib.cmakeFeature "CMAKE_BUILD_TYPE" "Release")
          (lib.cmakeFeature "CMAKE_PREFIX_PATH" "${./cmake/compat}")
        ];
        # monica-run is all the test suites need
        buildPhase = ''
          runHook preBuild
          cmake --build . --target monica-run
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          install -Dm755 monica-run -t $out/bin
          runHook postInstall
        '';
        meta.mainProgram = "monica-run";
      };
      # --- end TEMPORARY -----------------------------------------------------

      schemaDir = "share/monica/zalfmas_capnp_schemas";

      monica-odin = pkgs.clangStdenv.mkDerivation {
        pname = "monica-odin";
        version = self.shortRev or self.dirtyShortRev or "dev";
        src = ./odin;
        nativeBuildInputs = [
          odin
          pkgs.makeWrapper
        ];
        # Odin links via the cc-wrapped `clang`, which adds -L and an rpath for
        # each of these - no -extra-linker-flags needed, unlike odinw.py.
        buildInputs = [
          zeromq
          capnp-shim
          cr-math
        ];
        buildPhase = ''
          runHook preBuild
          export HOME=$TMPDIR
          mkdir -p build
          # Unoptimized, like `odinw.py build` without --release: that is what
          # the byte-identical CSV comparison against C++ validates.
          for t in monica-run monica-zmq-server monica-zmq-proxy monica-capnp-server monica-capnp-fbp-component monica-module; do
            odin build cmd/$t -out:build/$t
          done
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          install -Dm755 build/monica-* -t $out/bin
          mkdir -p $out/share/monica
          cp -r support/capnp/shim/external/mas_capnproto_schemas/zalfmas_capnp_schemas $out/${schemaDir}
          # The default schema path is relative to the exe, which doesn't hold
          # in the store layout.
          for t in monica-capnp-server monica-capnp-fbp-component; do
            wrapProgram $out/bin/$t --set-default MONICA_CAPNP_SCHEMA_DIR $out/${schemaDir}
          done
          runHook postInstall
        '';
      };
    in
    {
      packages.${system} = {
        inherit
          odin
          capnproto
          capnp-shim
          cr-math
          monica-odin
          monica-cpp-src # TEMPORARY, see above
          ;
        default = monica-odin;
      };

      # For the pixi build (odin/pixi.toml), which brings its own clang, python
      # and zeromq; this shell supplies what pixi doesn't: cmake/ninja and a C++
      # compiler for vcpkg's source build of Cap'n Proto, plus editor tooling.
      # clangStdenv: clang/clang++ als Standard-Compiler in der Shell
      devShells.${system}.default = pkgs.mkShell.override { stdenv = pkgs.clangStdenv; } {
        packages = with pkgs; [
          # Odin, same version as odinw.py's
          odin
          ols
          # C++
          cmake
          ninja
          pkg-config
          clang-tools # clangd, clang-format
          gdb
          lldb
        ];
        shellHook = ''
          export VCPKG_ROOT="''${VCPKG_ROOT:-$HOME/GitHub/vcpkg}"
        '';
      };
    };
}
