{
  description = "A template for Nix based C++ project setup.";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, ... }@inputs: inputs.utils.lib.eachSystem [
    "x86_64-linux" "i686-linux" "aarch64-linux" "x86_64-darwin"
  ] (system: let
    pkgs = import nixpkgs {
      inherit system;

      # Add overlays here if you need to override the nixpkgs
      # official packages.
      overlays = [];
      
      # config.allowUnfree = true;
    };
  in {
    devShells.default = pkgs.mkShell rec {
      name = "schesim";
      packages = with pkgs; [
        cmake
      ];
    };

    packages.default = pkgs.callPackage ./package.nix {};
  });
}
