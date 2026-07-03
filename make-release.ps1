$ErrorActionPreference = "Stop"

$version = "0.1.0"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$releaseRoot = Join-Path $root "release"
$bundleRoot = Join-Path $releaseRoot "rayquiro-$version"

if (Test-Path $bundleRoot) {
    Remove-Item -LiteralPath $bundleRoot -Recurse -Force
}

New-Item -ItemType Directory -Path $bundleRoot | Out-Null

Write-Host "[RayQuiro] Building rqio.exe"
& (Join-Path $root "build.ps1")

Write-Host "[RayQuiro] Building installer"
& (Join-Path $root "rqio.exe") build (Join-Path $root "examples\\rayquiro_installer.rq") -o (Join-Path $root "rayquiro_installer.exe")

Copy-Item -Path (Join-Path $root "rqio.exe") -Destination (Join-Path $bundleRoot "rqio.exe") -Force
Copy-Item -Path (Join-Path $root "rayquiro_installer.exe") -Destination (Join-Path $bundleRoot "rayquiro_installer.exe") -Force
Copy-Item -Path (Join-Path $root "README.md") -Destination (Join-Path $bundleRoot "README.md") -Force

if (Test-Path (Join-Path $root "include")) {
    Copy-Item -Path (Join-Path $root "include") -Destination (Join-Path $bundleRoot "include") -Recurse -Force
}

if (Test-Path (Join-Path $root "third_party")) {
    Copy-Item -Path (Join-Path $root "third_party") -Destination (Join-Path $bundleRoot "third_party") -Recurse -Force
}

if (Test-Path (Join-Path $root "toolchain")) {
    Copy-Item -Path (Join-Path $root "toolchain") -Destination (Join-Path $bundleRoot "toolchain") -Recurse -Force
}

if (Test-Path (Join-Path $root "vscode\\rayquiro")) {
    Copy-Item -Path (Join-Path $root "vscode\\rayquiro") -Destination (Join-Path $bundleRoot "vscode") -Recurse -Force
}

if (Test-Path (Join-Path $root "togithub")) {
    Copy-Item -Path (Join-Path $root "togithub") -Destination (Join-Path $bundleRoot "togithub") -Recurse -Force
}

Write-Host "[RayQuiro] Release bundle is ready at $bundleRoot"
