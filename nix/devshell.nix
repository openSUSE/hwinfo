{
  perSystem,
  pkgs,
  ...
}: pkgs.mkShell {
    packages = with pkgs; [
        perl
        perlPackages.XMLParser
        perlPackages.XMLWriter
    ];
}