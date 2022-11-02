{ stdenv }:

stdenv.mkDerivation {
  name = "zfs-fragmentation";
  src = ./.;
  allowSubstitutes = false;
}
