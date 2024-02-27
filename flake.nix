{
  description = "Schedsim is a C++ scheduling simulator";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, ... }@inputs: inputs.utils.lib.eachSystem [
    "x86_64-linux" "i686-linux" "aarch64-linux" "x86_64-darwin"
  ] (system: let
    pkgs = import nixpkgs {
      inherit system;
    };
  in {
  formatter.x86_64-linux = nixpkgs.legacyPackages.x86_64-linux.nixpkgs-fmt;
    devShells.default = pkgs.mkShell rec {
      name = "schesim";
      packages = with pkgs; [
        cmake
        doxygen
        graphviz
        ninja
        gtest
        gdb
	shellcheck
      ];
    };

    packages.default = pkgs.callPackage ./package.nix {};
  });
}
