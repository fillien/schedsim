{
  description = "Schedsim is a C++ scheduling simulator";

  inputs = {
    utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, ... }@inputs: inputs.utils.lib.eachSystem [
    "x86_64-linux"
    "i686-linux"
    "aarch64-darwin"
    "x86_64-darwin"
  ]
    (system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };
      in
      {
        formatter = pkgs.nixpkgs-fmt;
        devShells.default = pkgs.mkShell {
          name = "schedsim";
          packages = with pkgs; [
            (python312.withPackages (ps: with ps; [ pandas polars pyarrow ]))
            bash
            black
            clang-tools
            cmake
            doxygen
            gnuplot
            graphviz
            gtest
            lldb
            ninja
            shellcheck
            tracy
          ];
        };

        packages.default = pkgs.callPackage ./package.nix { };
      });
}
