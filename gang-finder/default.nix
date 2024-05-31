{ stdenv }:

stdenv.mkDerivation {
  name = "gang-finder";
  src = ./.;
  installPhase = ''
    mkdir -pv $out/bin/
    cp gang-finder $out/bin/
  '';
}
