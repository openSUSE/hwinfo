{
  description = "Hardware information tool";

  inputs = {
      blueprint = {
        url = "github:numtide/blueprint";
        inputs.nixpkgs.follows = "nixpkgs";
        inputs.systems.follows = "systems";
      };
      systems.url = "github:nix-systems/default";
      nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    };

   outputs = inputs:
      inputs.blueprint {
        prefix = "nix/";
        inherit inputs;
        systems = [
          "aarch64-linux"
          "riscv64-linux"
          "x86_64-linux"
        ];
      };
}
