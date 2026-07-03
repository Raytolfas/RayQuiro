$ErrorActionPreference = 'Stop'

param(
    [string]$SourceRoot = '',
    [string]$TargetRoot = '.\toolchain\mingw64'
)

if ([string]::IsNullOrWhiteSpace($SourceRoot)) {
    $candidates = @(
        "$env:MSYS2_ROOT\mingw64",
        "C:\msys64\mingw64",
        "G:\msys2\mingw64"
    ) | Where-Object { $_ -and (Test-Path $_) }

    if ($candidates.Count -eq 0) {
        throw "Could not find a local MinGW/MSYS2 install. Pass -SourceRoot manually."
    }

    $SourceRoot = $candidates[0]
}

$source = Resolve-Path $SourceRoot
$target = Resolve-Path . -ErrorAction SilentlyContinue
if (-not $target) {
    $target = Get-Location
}

$resolvedTarget = Join-Path $target $TargetRoot
New-Item -ItemType Directory -Force -Path $resolvedTarget | Out-Null

$items = @(
    'bin',
    'lib',
    'libexec',
    'x86_64-w64-mingw32'
)

foreach ($item in $items) {
    $from = Join-Path $source $item
    if (-not (Test-Path $from)) {
        Write-Warning "Skipping missing path: $from"
        continue
    }

    $to = Join-Path $resolvedTarget $item
    Write-Host "[RayQuiro] Copying $from -> $to"
    Copy-Item -Path $from -Destination $to -Recurse -Force
}

Write-Host "[RayQuiro] Bundled toolchain is ready in $resolvedTarget"
