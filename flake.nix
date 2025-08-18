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

        schedsim = pkgs.stdenv.mkDerivation {
          name = "schedsim";
          src = ./.;
          buildInputs = with pkgs; [
            cmake
            ninja
            doxygen
            graphviz
            gtest
            llvmPackages.openmp
            (python312.withPackages (ps: with ps; [
              pybind11
              pybind11-stubgen
              sphinx
              furo
              breathe
            ]))
          ];
          cmakeFlags = [ "-GNinja" "-DCMAKE_BUILD_TYPE=Release" ];
          doCheck = false;
        };

        schedsimTest = pkgs.stdenv.mkDerivation {
          name = "schedsim-test";
          src = ./.;
          buildInputs = with pkgs; [
            cmake
            ninja
            doxygen
            graphviz
            gtest
            (python312.withPackages (ps: with ps; [
              pybind11
              pybind11-stubgen
              sphinx
              furo
              breathe
            ]))
          ];
          cmakeFlags = [ "-GNinja" "-DCMAKE_BUILD_TYPE=Debug" ];
          doCheck = true;
          buildPhase = ''
            cmake --build . --target test
          '';
          checkPhase = ''
            ctest --output-on-failure
          '';
        };

        devShellInputs = with pkgs; [
          (python312.withPackages (ps: with ps; [
            pip
            jupytext
            pybind11
            pandas
            polars
            pyarrow
            matplotlib
            plotly
            jupyter
            jupyterlab
            jupyter-nbextensions-configurator
            jupyterlab-execute-time
            pybind11-stubgen
            sphinx
            furo
            breathe
          ]))
          bash
          black
          clang-tools
          gource
          cmake
          doxygen
          gnuplot
          graphviz
          gtest
          jd-diff-patch
          lldb
          ninja
          shellcheck
          tracy
          sphinx
          llvmPackages.openmp
          llvmPackages.clang
        ];
      in
      {
        packages.default = schedsim;
        checks.default = schedsimTest;
        devShells.default = pkgs.mkShell {
          name = "schedsim-shell";
          buildInputs = devShellInputs;
        };
        formatter = pkgs.nixpkgs-fmt;
      });
}
