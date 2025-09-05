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

        schedsim = pkgs.llvmPackages.stdenv.mkDerivation {
          name = "schedsim";
          src = ./.;
          buildInputs = with pkgs; [
            cmake
            ninja
            doxygen
            graphviz
            gtest
            llvmPackages.openmp
            llvmPackages.clang
            (python312.withPackages (ps: with ps; [
              pybind11
              pybind11-stubgen
              sphinx
              furo
              breathe
            ]))
          ];
          cmakeFlags = [ "-GNinja" "-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_C_COMPILER=clang" "-DCMAKE_CXX_COMPILER=clang++" ];
          doCheck = false;
        };

        muslStaticPkgs = pkgs.pkgsMusl.pkgsStatic;
        schedsimStatic = muslStaticPkgs.stdenv.mkDerivation {
          pname = "schedsim-static";
          version = "unstable-${self.shortRev or "dev"}";
          src = ./.;

          # Native tools come from the host pkgs, not the target pkgs
          nativeBuildInputs = [ pkgs.cmake pkgs.ninja pkgs.pkg-config ];

          # Only add real C/C++ library deps here if your code needs them.
          buildInputs = [ ];

          cmakeFlags = [
            "-GNinja"
            "-DCMAKE_BUILD_TYPE=Release"
            "-DBUILD_SHARED_LIBS=OFF"
            "-DCMAKE_EXE_LINKER_FLAGS=-static"
            "-DCMAKE_FIND_LIBRARY_SUFFIXES=.a"
          ];

          # If you rely on OpenMP, enable it (GCC/libgomp). Often CMake finds it;
          # if not, uncomment:
          # NIX_CFLAGS_COMPILE = [ "-fopenmp" ];
          # NIX_LDFLAGS = [ "-fopenmp" "-static" ];

          enableParallelBuilding = true;
          doCheck = false;
        };

        schedsimTest = pkgs.llvmPackages.stdenv.mkDerivation {
          name = "schedsim-test";
          src = ./.;
          buildInputs = with pkgs; [
            cmake
            ninja
            doxygen
            graphviz
            gtest
            llvmPackages.clang
            (python312.withPackages (ps: with ps; [
              pybind11
              pybind11-stubgen
              sphinx
              furo
              breathe
            ]))
          ];
          cmakeFlags = [ "-GNinja" "-DCMAKE_BUILD_TYPE=Debug" "-DCMAKE_C_COMPILER=clang" "-DCMAKE_CXX_COMPILER=clang++" ];
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
        packages.schedsim-static = schedsimStatic;
        checks.default = schedsimTest;
        devShells.default = pkgs.mkShell {
          name = "schedsim-shell";
          buildInputs = devShellInputs;
        };
        formatter = pkgs.nixpkgs-fmt;
      });
}
