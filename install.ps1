param(
    [string]$ManifestUrl = "https://raw.githubusercontent.com/Raytolfas/Assets/refs/heads/main/RayQuiro/update.json",
    [string]$InstallRoot = $(if ($env:ProgramFiles) { Join-Path $env:ProgramFiles "RayQuiro" } else { "C:\Program Files\RayQuiro" })
)

$ErrorActionPreference = "Stop"
$host.UI.RawUI.WindowTitle = "RayQuiro Installer"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

function Write-Step([string]$text) {
    Write-Host "[RayQuiro Installer] $text"
}

function Download-File([string]$url, [string]$outFile) {
    try {
        Invoke-WebRequest -Uri $url -OutFile $outFile -UseBasicParsing
        return
    } catch {
        if (Get-Command curl.exe -ErrorAction SilentlyContinue) {
            & curl.exe -L --fail --silent --show-error $url -o $outFile
            if ($LASTEXITCODE -eq 0) {
                return
            }
        }
        throw
    }
}

function Add-ToUserPath([string]$targetPath) {
    $current = [Environment]::GetEnvironmentVariable("Path", "User")
    if ([string]::IsNullOrWhiteSpace($current)) {
        [Environment]::SetEnvironmentVariable("Path", $targetPath, "User")
        return $true
    }

    $parts = $current.Split(";") | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    foreach ($part in $parts) {
        if ($part.TrimEnd("\") -ieq $targetPath.TrimEnd("\")) {
            return $true
        }
    }

    [Environment]::SetEnvironmentVariable("Path", "$current;$targetPath", "User")
    return $true
}

function Ask-YesNo([string]$prompt, [bool]$defaultYes = $true) {
    $suffix = if ($defaultYes) { "[Y/n]" } else { "[y/N]" }
    $answer = Read-Host "$prompt $suffix"
    if ([string]::IsNullOrWhiteSpace($answer)) {
        return $defaultYes
    }
    return $answer.Trim().ToLowerInvariant().StartsWith("y")
}

$installBin = Join-Path $InstallRoot "bin"
$installModules = Join-Path $InstallRoot "modules"
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) "rayquiro-installer"
$downloadPath = Join-Path $tempRoot "rqio.exe"
$manifestPath = Join-Path $tempRoot "update.json"

if (-not (Ask-YesNo "Install RayQuiro to $InstallRoot?" $true)) {
    Write-Step "Installation cancelled"
    return
}

Write-Step "Preparing temporary folder"
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

Write-Step "Downloading update manifest"
Download-File $ManifestUrl $manifestPath
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json

$downloadUrl = $manifest.downloadUrl
if ([string]::IsNullOrWhiteSpace($downloadUrl)) {
    $downloadUrl = $manifest.download_url
}

if ([string]::IsNullOrWhiteSpace($downloadUrl)) {
    throw "update.json does not contain downloadUrl or download_url"
}

Write-Step "Downloading rqio.exe"
Download-File $downloadUrl $downloadPath

Write-Step "Creating install folders"
New-Item -ItemType Directory -Force -Path $installBin | Out-Null
New-Item -ItemType Directory -Force -Path $installModules | Out-Null

Write-Step "Installing rqio.exe to $installBin"
$installedCli = Join-Path $installBin "rqio.exe"
Copy-Item -LiteralPath $downloadPath -Destination $installedCli -Force

Write-Step "Updating user PATH"
Add-ToUserPath $installBin | Out-Null

$defaultModules = @("engine", "web", "app")
if (Ask-YesNo "Install optional RayQuiro modules (engine, web, app)?" $true) {
    foreach ($module in $defaultModules) {
        Write-Step "Installing module rayquiro.$module"
        & $installedCli install "rayquiro.$module"
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to install rayquiro.$module"
        }
    }
}

Write-Step "Installation complete"
Write-Host ""
Write-Host "RayQuiro was installed to: $installBin"
Write-Host "Modules path: $installModules"
Write-Host "Restart your terminal, then run:"
Write-Host "rqio version"
