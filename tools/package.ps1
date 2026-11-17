<#
.SYNOPSIS
    Builds the submission ZIP: 24I-3071_24I-3000_BSE-5B.zip
    (Muhammad Wasay Tariq 24I-3071, Syed Muhammad Rayyan Hassan 24I-3000; section BSE-5B)

.DESCRIPTION
    1. Configures and builds the Release configuration (all targets, including
       the GUI once src/app exists).
    2. Runs smartrace_tests; stops if any test fails.
    3. Stages a clean copy of the sources, data, results and report, plus a
       bin/ folder with the Release executables and data/ next to them.
    4. Zips it as build/package/24I-3071_24I-3000_<Section>.zip (default section BSE-5B).
    5. With -Verify: extracts the ZIP into an empty temporary folder,
       configures, builds and runs the tests there (the brief: check the ZIP
       for missing files and corruption before submitting).

    Run from the repository root in PowerShell:
        powershell -ExecutionPolicy Bypass -File tools/package.ps1 -Verify

.PARAMETER Section
    The group's section. Defaults to BSE-5B (Software Engineering).

.PARAMETER Verify
    Also rebuild and test from the extracted ZIP in a clean folder.

.PARAMETER SkipBuild
    Package whatever is already in build/Release (no build, no tests).
#>
param(
    [string]$Section = 'BSE-5B',
    [switch]$Verify,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $root

$name = "24I-3071_24I-3000_$Section"
$generator = 'Visual Studio 18 2026'
$packageDir = Join-Path $root 'build\package'
$stage = Join-Path $packageDir $name
$zip = Join-Path $packageDir "$name.zip"

function Step([string]$text)
{
    Write-Host ''
    Write-Host "== $text" -ForegroundColor Cyan
}

function Invoke-Checked([string]$exe, [string[]]$arguments)
{
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "$exe $($arguments -join ' ') failed with exit code $LASTEXITCODE" }
}

# ---------------------------------------------------------------- build + test
if (-not $SkipBuild)
{
    Step 'Configure and build (Release)'
    Invoke-Checked 'cmake' @('-S', '.', '-B', 'build', '-G', $generator, '-A', 'x64')
    Invoke-Checked 'cmake' @('--build', 'build', '--config', 'Release')

    Step 'Run tests (Release)'
    Invoke-Checked (Join-Path $root 'build\Release\smartrace_tests.exe') @()
}

# ---------------------------------------------------------------- stage
Step "Stage $name"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
if (Test-Path $zip) { Remove-Item -Force $zip }
New-Item -ItemType Directory -Force $stage | Out-Null

$folders = @('src', 'cli', 'tests', 'bench', 'tools', 'data', 'results', 'third_party', 'report')
foreach ($f in $folders)
{
    if (Test-Path (Join-Path $root $f))
    {
        Copy-Item -Recurse (Join-Path $root $f) (Join-Path $stage $f)
    }
}
foreach ($f in @('CMakeLists.txt', 'README.md', '.gitignore'))
{
    if (Test-Path (Join-Path $root $f)) { Copy-Item (Join-Path $root $f) (Join-Path $stage $f) }
}

# Python caches and editor folders never belong in the submission.
Get-ChildItem -Path $stage -Recurse -Force -Directory -Include '__pycache__', '.vs', '.vscode' |
    Remove-Item -Recurse -Force

# bin/: Release executables with data/ next to them, ready to run.
$bin = Join-Path $stage 'bin'
New-Item -ItemType Directory -Force $bin | Out-Null
$exes = @('smartrace.exe', 'smartrace_cli.exe', 'smartrace_bench.exe', 'smartrace_tests.exe')
foreach ($exe in $exes)
{
    $path = Join-Path $root "build\Release\$exe"
    if (Test-Path $path) { Copy-Item $path $bin }
}
Get-ChildItem -Path (Join-Path $root 'build\Release') -Filter '*.dll' -ErrorAction SilentlyContinue |
    ForEach-Object { Copy-Item $_.FullName $bin }
Copy-Item -Recurse (Join-Path $root 'data') (Join-Path $bin 'data')

# ---------------------------------------------------------------- warnings
$warnings = @()
if (-not (Test-Path (Join-Path $bin 'smartrace.exe'))) { $warnings += 'GUI executable smartrace.exe is missing (src/app not built yet?)' }
if (-not (Get-ChildItem -Path (Join-Path $stage 'report') -Filter '*.pdf' -ErrorAction SilentlyContinue))
{
    $warnings += 'No PDF in report/ -- the brief requires the technical report as a PDF'
}
if (-not (Test-Path (Join-Path $stage 'results\raw'))) { $warnings += 'results/raw is missing -- run smartrace_bench all' }

# ---------------------------------------------------------------- zip
Step "Zip -> $zip"
# .NET's ZipFile writes standard '/' entry paths (PowerShell 5.1's Compress-Archive writes '\',
# which some unzip tools on other systems turn into odd file names).
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($stage, $zip, [System.IO.Compression.CompressionLevel]::Optimal, $false)
$sizeMb = [math]::Round((Get-Item $zip).Length / 1MB, 2)
Write-Host "Wrote $zip ($sizeMb MB)"

# ---------------------------------------------------------------- verify
if ($Verify)
{
    Step 'Verify: extract into an empty folder, build and test there'
    # Under build/ (git-ignored), not %TEMP%: MSBuild warns about output folders under the temp directory.
    $check = Join-Path $packageDir ("zipcheck_" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force $check | Out-Null
    try
    {
        Expand-Archive -Path $zip -DestinationPath $check
        Push-Location $check
        try
        {
            Invoke-Checked 'cmake' @('-S', '.', '-B', 'build', '-G', $generator, '-A', 'x64')
            Invoke-Checked 'cmake' @('--build', 'build', '--config', 'Release')
            Invoke-Checked (Join-Path $check 'build\Release\smartrace_tests.exe') @()
            foreach ($exe in $exes)
            {
                if ((Test-Path (Join-Path $bin $exe)) -and -not (Test-Path (Join-Path $check "bin\$exe")))
                {
                    throw "bin\$exe is missing from the extracted ZIP"
                }
            }
        }
        finally
        {
            Pop-Location
        }
        Write-Host 'Clean-folder build and tests: OK' -ForegroundColor Green
    }
    finally
    {
        Remove-Item -Recurse -Force $check -ErrorAction SilentlyContinue
    }
}

if ($warnings.Count -gt 0)
{
    Step 'Warnings'
    foreach ($w in $warnings) { Write-Host "  - $w" -ForegroundColor Yellow }
}
