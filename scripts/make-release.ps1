# Build Release|x64 and pack portable zip (+ Inno Setup if installed).
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$sln = Join-Path $root "Quickshot.sln"
$msbuild = @(
    "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $msbuild) { throw "MSBuild not found" }

$dist = Join-Path $root "dist"
$portableDir = Join-Path $dist "Quickshot-portable"
New-Item -ItemType Directory -Force -Path $dist | Out-Null
if (Test-Path $portableDir) { Remove-Item $portableDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $portableDir | Out-Null

Get-Process Quickshot -ErrorAction SilentlyContinue | Stop-Process -Force

Write-Host "Building Release|x64..."
& $msbuild $sln /p:Configuration=Release /p:Platform=x64 /m /v:minimal
if ($LASTEXITCODE -ne 0) { throw "MSBuild failed" }

$exe = Join-Path $root "build\x64\Release\Quickshot.exe"
if (-not (Test-Path $exe)) { throw "Release EXE not found: $exe" }

Copy-Item $exe $portableDir
Copy-Item (Join-Path $root "installer\settings.ini.default") (Join-Path $portableDir "settings.ini")

$ver = "1.2.1"
$zip = Join-Path $dist "Quickshot-$ver-portable-x64.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $portableDir "*") -DestinationPath $zip -Force
Write-Host "Portable: $zip"

$iscc = @(
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if ($iscc) {
    Write-Host "Building installer with Inno Setup..."
    & $iscc (Join-Path $root "installer\Quickshot.iss")
    Write-Host "Installer written to dist\"
} else {
    Write-Host "Inno Setup not found — Setup.exe skipped (portable zip ready)."
}
