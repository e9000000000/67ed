param(
    [int]$Megabytes = 100,
    [switch]$OneLine,
    [string]$Fixture = '',
    [string]$Log = (Join-Path $env:TEMP 'opencode\67ed-bench.log')
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root 'build\67ed.exe'
if (-not (Test-Path $exe)) {
    throw "Missing $exe. Run .\build.bat first."
}

if (-not $Fixture) {
    $suffix = if ($OneLine) { '-one-line' } else { '' }
    $Fixture = Join-Path $env:TEMP "opencode\67ed-bench-$Megabytes`mb$suffix.txt"
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Fixture) | Out-Null

if (-not (Test-Path $Fixture)) {
    $fixtureLog = Join-Path $env:TEMP 'opencode\67ed-fixture.log'
    $args = "--make-fixture `"$Fixture`" --mb $Megabytes --log `"$fixtureLog`""
    if ($OneLine) { $args += ' --one-line' }
    $generated = Start-Process -FilePath $exe -ArgumentList $args -Wait -PassThru -WindowStyle Hidden
    if ($generated.ExitCode -ne 0) {
        Get-Content $fixtureLog -ErrorAction SilentlyContinue
        throw "Fixture generation failed (exit code $($generated.ExitCode))."
    }
}

if (Test-Path $Log) { Remove-Item $Log -Force }
$args = "--bench `"$Fixture`" --log `"$Log`""
$process = Start-Process -FilePath $exe -ArgumentList $args -Wait -PassThru -WindowStyle Hidden
if (-not (Test-Path $Log)) {
    throw "67ed bench produced no log (exit code $($process.ExitCode))."
}
Write-Output "fixture=$Fixture"
Get-Content $Log
exit $process.ExitCode
