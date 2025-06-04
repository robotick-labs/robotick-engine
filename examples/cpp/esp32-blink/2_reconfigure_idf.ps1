# Check if idf.py is available
if (!(Get-Command "idf.py" -ErrorAction SilentlyContinue)) {
    Write-Error "idf.py not found. Please ensure ESP-IDF is properly installed and sourced."
    exit 1
}

# Run reconfigure with error handling
idf.py reconfigure
if ($LASTEXITCODE -ne 0) {
    Write-Error "ESP-IDF reconfiguration failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Write-Host "ESP-IDF reconfiguration completed successfully."