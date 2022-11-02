{
  outputs = { nixpkgs, self }:
  {
    packages.x86_64-linux = let
      pkgs = (import nixpkgs { system = "x86_64-linux"; });
    in {
      zfs-util = pkgs.stdenv.mkDerivation {
        src = ./.;
        name = "zfs-util";
      };
      spacemap-exporter = pkgs.stdenv.mkDerivation {
        name = "spacemap-exporter";
        src = ./spacemap-exporter;
        buildInputs = [ pkgs.zfs ];
        nativeBuildInputs = [ pkgs.pkgconfig ];
      };
    };
  };
}
