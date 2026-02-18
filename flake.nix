{
  description = "Schedsim is a C++ scheduling simulator";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";
    utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, utils, ... }:
    utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        lib = pkgs.lib;

        schedsim = pkgs.llvmPackages.stdenv.mkDerivation {
          name = "schedsim";
          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
          ];

          buildInputs = with pkgs; [
            gtest
            rapidjson  # Header-only JSON library
          ];

          cmakeFlags = [
            "-GNinja"
            "-DCMAKE_BUILD_TYPE=Release"
            "-DCMAKE_C_COMPILER=clang"
            "-DCMAKE_CXX_COMPILER=clang++"
            "-DBUILD_PYTHON=OFF"
          ];

          doCheck = false;
        };

        schedsim-python = schedsim.overrideAttrs (old: {
          name = "schedsim-python";

          nativeBuildInputs = old.nativeBuildInputs ++ [ pkgs.python312Packages.nanobind ];

          buildInputs = old.buildInputs ++ [ pkgs.python312 ];

          cmakeFlags = builtins.filter (f: f != "-DBUILD_PYTHON=OFF") old.cmakeFlags
            ++ [ "-DBUILD_PYTHON=ON" ];
        });

        schedsimTest = schedsim.overrideAttrs (old: {
          name = "schedsim-test";

          cmakeFlags = builtins.filter (f: !lib.hasPrefix "-DCMAKE_BUILD_TYPE" f) old.cmakeFlags
            ++ [ "-DCMAKE_BUILD_TYPE=Debug" ];

          doCheck = true;

          checkPhase = ''
            ctest --output-on-failure
          '';
        });
      in
      {
        packages.default = schedsim;
        packages.schedsim-python = schedsim-python;

        checks.default = schedsimTest;

        devShells.default = pkgs.mkShell.override { stdenv = pkgs.llvmPackages.stdenv; } {
          name = "schedsim-shell";

          packages = with pkgs; [
            # Build system
            cmake
            ninja

            # Compiler and tools
            llvmPackages.clang
            clang-tools
            lldb

            # Libraries
            gtest
            rapidjson

            # Python bindings (nanobind)
            python312Packages.nanobind

            # Profiling
            tracy

            # Web visualization (viz/)
            nodejs_22
            nodePackages.npm

            # Python environment
            (python312.withPackages (ps: with ps; [
              pip
              jupytext
              pandas
              polars
              pyarrow
              matplotlib
              plotly
              jupyter
              jupyterlab
              scipy
            ]))
            black

            # Utilities
            gnuplot
            jd-diff-patch
          ];
        };

        formatter = pkgs.nixpkgs-fmt;
      });
}
