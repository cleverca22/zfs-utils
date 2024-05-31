{
  inputs = {
    utils.url = "github:numtide/flake-utils";
  };
  outputs = { nixpkgs, self, utils }:
  utils.lib.eachSystem [ "x86_64-linux" "i686-linux" ] (system:
  let
    pkgs = import nixpkgs { inherit system; };
    wrap = drv: drv // { type = "derivation"; outPath = drv.out; outputName = "out"; };
  in {
    packages = {
      gang-finder = pkgs.callPackage ./gang-finder {};
      zfs-fragmentation = pkgs.callPackage ./fragmentation {};
      txg-watcher = pkgs.callPackage ./txg-watcher {};
      zfs-util = pkgs.stdenv.mkDerivation {
        src = ./.;
        name = "zfs-util";
        dontStrip = true;
      };
      test = pkgs.runCommand "test" {} ''
      '';
      #test2 = wrap (builtins.derivationStrict {
      #  name = "foo";
      #  builder = "/bin/sh";
      #  outputs = [ "out" "dev" ];
      #  args = [ ./foo.sh ./flake.lock "bar" ];
      #  system = "x86_64-linux";
      #});
      spacemap-exporter = pkgs.stdenv.mkDerivation {
        name = "spacemap-exporter";
        src = ./spacemap-exporter;
        buildInputs = [ pkgs.zfs ];
        nativeBuildInputs = [ pkgs.pkgconfig ];
      };
    };
  });
}
