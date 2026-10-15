{
  description = "Odin-Entwicklungsumgebung";

  # Odin entwickelt sich schnell -> bei Bedarf auf "nixpkgs-unstable" umstellen
  inputs.nixpkgs.url = "nixpkgs";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [ odin ols gdb ];
      };
    };
}
