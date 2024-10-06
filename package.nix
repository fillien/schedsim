{ pkgs, lib, stdenv, fetchFromGitHub }:

stdenv.mkDerivation {
  name = "schedsim";
  src = ./.;
  buildInputs = with pkgs; [ cmake doxygen graphviz gtest ];
}
