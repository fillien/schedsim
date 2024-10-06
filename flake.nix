{
  description = "Schedsim is a C++ scheduling simulator";

  inputs = {
    utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, ... }@inputs: inputs.utils.lib.eachSystem [
    "x86_64-linux"
    "i686-linux"
    "aarch64-linux"
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
          name = "schesim";
          packages = with pkgs; [
            bash
            cmake
            doxygen
            graphviz
            ninja
            gtest
            gdb
            shellcheck
            black
            tracy
            black
            (python311.withPackages (ps: with ps; [ pandas ]))
          ];
        };

        packages.default = pkgs.callPackage ./package.nix { };
      });
}
