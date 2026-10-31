param(
    [string]$Log = (Join-Path $env:TEMP 'opencode\67ed-selftest.log')
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root 'build\67ed.exe'
if (-not (Test-Path $exe)) {
    throw "Missing $exe. Run .\build.bat first."
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Log) | Out-Null
if (Test-Path $Log) { Remove-Item $Log -Force }

$arguments = "--selftest --log `"$Log`""
$process = Start-Process -FilePath $exe -ArgumentList $arguments -Wait -PassThru -WindowStyle Hidden
if (-not (Test-Path $Log)) {
    throw "67ed selftest produced no log (exit code $($process.ExitCode))."
}
Get-Content $Log
exit $process.ExitCode
