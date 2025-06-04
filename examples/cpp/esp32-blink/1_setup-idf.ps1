$componentsDir = "components"
$symlinkPath = Join-Path $componentsDir "robotick-engine"
$symlinkTarget = "..\..\..\cpp"

# Only do admin work if symlink doesn't already exist
if (-not (Test-Path $symlinkPath)) {

    # Check for admin rights
    if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
            [Security.Principal.WindowsBuiltInRole] "Administrator")) {

        $pwd = Get-Location

        # Create temp script for the privileged symlink setup
        $tempScriptPath = [System.IO.Path]::GetTempFileName() -replace '\.tmp$', '.ps1'
        @"
Set-Location -LiteralPath "$pwd"

if (-not (Test-Path "$componentsDir")) {
    New-Item -ItemType Directory -Path "$componentsDir" | Out-Null
    Write-Host "Created missing folder: $componentsDir"
}

if (-not (Test-Path "$symlinkPath")) {
    New-Item -ItemType SymbolicLink -Path "$symlinkPath" -Target "$symlinkTarget"
    Write-Host "Symlink created: $symlinkPath -> $symlinkTarget"
}
else {
    Write-Host "Symlink already exists: $symlinkPath"
}
"@ | Set-Content -Path $tempScriptPath -Encoding UTF8

        # Run elevated script and wait
        Start-Process powershell -Verb RunAs -ArgumentList "-NoProfile", "-ExecutionPolicy Bypass", "-File `"$tempScriptPath`"" -Wait

        # Clean up
        Remove-Item $tempScriptPath -Force
    }
    else {
        # Already admin — do it directly
        if (-not (Test-Path $componentsDir)) {
            New-Item -ItemType Directory -Path $componentsDir | Out-Null
            Write-Host "Created missing folder: $componentsDir"
        }

        if (-not (Test-Path $symlinkPath)) {
            New-Item -ItemType SymbolicLink -Path $symlinkPath -Target $symlinkTarget
            Write-Host "Symlink created: $symlinkPath -> $symlinkTarget"
        }
    }
}
else {
    Write-Host "Symlink already exists: $symlinkPath"
}

# Always run ESP-IDF setup (in non-admin context)
& "C:/Espressif/frameworks/esp-idf-v5.4.1/export.ps1"
