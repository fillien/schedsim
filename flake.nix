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
          buildInputs = with pkgs; [ cmake ninja doxygen graphviz gtest ];
          cmakeFlags = [ "-GNinja" ];
          doCheck = false;
        };

        schedsimTest = pkgs.stdenv.mkDerivation {
          name = "schedsim-test";
          src = ./.;
          buildInputs = with pkgs; [ cmake ninja doxygen graphviz gtest ];
          cmakeFlags = [ "-GNinja" "-DCMAKE_BUILD_TYPE=Debug" ];
          doCheck = true;
          buildPhase = ''
            cmake --build . --target tests
            cmake --build . --target schedview_tests
          '';
          checkPhase = ''
            ctest --output-on-failure
          '';
        };

        devShellInputs = with pkgs; [
          (python313.withPackages (ps: with ps; [ pandas polars pyarrow matplotlib jupyter ]))
          bash
          black
          clang-tools
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
