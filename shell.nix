{ pkgs ? import <nixpkgs> { } }:

with pkgs;
with callPackage ./lib.nix { };
terraShell {
  buildInputs = [ pkgs.libiconv pkgs.glibc.dev ];
  TERRA_PATH = "./?.t;./?/init.t;deps/?.t;deps/?.init.t";
  LUA_PATH = "./?.lua;deps/?.lua";
}
