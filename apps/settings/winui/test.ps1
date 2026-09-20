param([string]$Executable)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if (-not $Executable) { $Executable = Join-Path $root 'out/build/settings-winui/x64/Release/stroke_settings.exe' }
$state = Join-Path $root ('out/tests/settings-winui/' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $state | Out-Null
$process = Start-Process -FilePath $Executable -ArgumentList @('--self-test', ('"' + $state + '"')) -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit(30000)) { $process.Kill(); throw 'The isolated settings smoke test timed out.' }
$report = Join-Path $state 'result.txt'
if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) {
    if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report }
    throw "Settings smoke test failed: $($process.ExitCode)"
}
Get-Content -LiteralPath $report
