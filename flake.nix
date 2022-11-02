{
  inputs = {
    utils.url = "github:numtide/flake-utils";
  };
  outputs = { nixpkgs, self, utils }:
  utils.lib.eachSystem [ "x86_64-linux" "i686-linux" ] (system:
  let
    pkgs = import nixpkgs { inherit system; };
  in {
    packages = {
      zfs-fragmentation = pkgs.callPackage ./fragmentation {};
      txg-watcher = pkgs.callPackage ./txg-watcher {};
      spacemap-exporter = pkgs.stdenv.mkDerivation {
        name = "spacemap-exporter";
        src = ./spacemap-exporter;
        buildInputs = [ pkgs.zfs ];
        nativeBuildInputs = [ pkgs.pkgconfig ];
      };
    };
  });
}
