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
        devShells.default = pkgs.mkShell rec {
          name = "schedsim";
          packages = with pkgs; [
            bash
            cmake
            doxygen
            graphviz
            ninja
            gtest
            lldb
            shellcheck
            gnuplot
            black
            tracy
            (python312.withPackages (ps: with ps; [ pandas polars pyarrow ]))
          ];
        };

        packages.default = pkgs.callPackage ./package.nix { };
      });
}
