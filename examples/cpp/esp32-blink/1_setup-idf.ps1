# Check for admin rights
if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole] "Administrator")) {

    $script = $MyInvocation.MyCommand.Path
    $pwd = Get-Location

    Start-Process powershell -Verb RunAs -ArgumentList @(
        "-NoProfile",
        "-ExecutionPolicy Bypass",
        "-Command `"cd '$pwd'; & '$script'`""
    )
    exit
}

# --- Now running as admin ---

# Ensure components folder exists
$componentsDir = "components"
if (-not (Test-Path $componentsDir)) {
    New-Item -ItemType Directory -Path $componentsDir | Out-Null
    Write-Host "Created missing folder: $componentsDir"
}

# Create symlink
$symlinkPath = Join-Path $componentsDir "robotick-engine"
$symlinkTarget = "..\..\..\cpp"

if (-not (Test-Path $symlinkPath)) {
    New-Item -ItemType SymbolicLink -Path $symlinkPath -Target $symlinkTarget
    Write-Host "Symlink created: $symlinkPath -> $symlinkTarget"
}
else {
    Write-Host "Symlink already exists: $symlinkPath"
}

# Set up ESP-IDF
& "C:/Espressif/frameworks/esp-idf-v5.4.1/export.ps1"
