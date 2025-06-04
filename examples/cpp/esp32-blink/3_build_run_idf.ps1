idf.py build

if ($LASTEXITCODE -ne 0) {
    Write-Error "ESP-IDF build failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

# Flash to device and start monitor
idf.py -p COM5 flash
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to flash firmware"
    exit $LASTEXITCODE
}

idf.py monitor
