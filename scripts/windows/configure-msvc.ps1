$ErrorActionPreference = "Stop"

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$buildDirectory = Join-Path $projectRoot "build/x64"

Write-Host "Generating the Visual Studio 2022 MSVC project..."
cmake -S $projectRoot `
    -B $buildDirectory `
    -G "Visual Studio 17 2022" `
    -T v143 `
    -A x64 `
    -DSUPPRESS_WARNINGS=ON

Write-Host ""
Write-Host "Visual Studio project generated:"
Write-Host (Join-Path $buildDirectory "Ship.sln")
