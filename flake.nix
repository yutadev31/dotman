{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        packages = with pkgs; [
          cmake
          ninja
          clang
          clang-tools
        ];
      in
      {
        devShells.default = pkgs.mkShell {
          inherit packages;
        };
      }
    );
}
