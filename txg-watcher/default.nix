{ stdenv }:

stdenv.mkDerivation {
  name = "zfs-util";
  src = ./.;
}
