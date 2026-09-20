param([string]$Iscc)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Set-Location -LiteralPath $projectRoot
if (-not $Iscc) {
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA 'Programs/Inno Setup 6/ISCC.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6/ISCC.exe'),
        (Join-Path $env:ProgramFiles 'Inno Setup 6/ISCC.exe'))
    $Iscc = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $Iscc) { throw 'Install Inno Setup 6.7.3 or pass -Iscc.' }
function Invoke-BuildTool([string]$File, [string[]]$ToolArgs) {
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $File
    $start.UseShellExecute = $false
    $start.WorkingDirectory = $projectRoot
    $clean = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in [Environment]::GetEnvironmentVariables('Process').GetEnumerator()) { $clean[$entry.Key] = $entry.Value }
    $start.Environment.Clear()
    foreach ($entry in $clean.GetEnumerator()) { $start.Environment[$entry.Key] = $entry.Value }
    foreach ($argument in $ToolArgs) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) { throw "$File failed: $($process.ExitCode)" }
}
foreach ($arch in @('x64', 'x86')) {
    Invoke-BuildTool 'cmake.exe' @('--preset', "package-$arch")
    Invoke-BuildTool 'cmake.exe' @('--build', "out/build/package-$arch", '--config', 'Release', '--parallel', '1')
    Invoke-BuildTool 'ctest.exe' @('--test-dir', "out/build/package-$arch", '-C', 'Release', '--output-on-failure')
    # The current decoder rejects every unsupported format before staging or packaging.
    $dictTool = Join-Path $projectRoot "out/build/package-$arch/tools/dict_builder/Release/stroke_dict_builder.exe"
    $frequency = Join-Path $projectRoot 'data/generated/conway-v2.0.2-traditional/frequency'
    foreach ($required in @('character-score.tsv', 'NOTICE.txt', 'sources/tsi.csv', 'sources/provenance.json', 'sources/COPYING.LGPL-2.1.txt')) {
        if (-not (Test-Path -LiteralPath (Join-Path $frequency $required) -PathType Leaf)) {
            throw "Missing frequency data or attribution: $required"
        }
    }
    # Include corresponding conversion sources with the data, refreshed for this build.
    Compress-Archive -Path src,tools,cmake,apps,tests,CMakeLists.txt,CMakePresets.json,LICENSE,THIRD_PARTY_NOTICES.md -DestinationPath (Join-Path $frequency 'converter-source.zip') -Force
    Invoke-BuildTool $dictTool @('inspect', '--index', 'data/generated/conway-v2.0.2-traditional/dictionary.sidx')
    $destination = Join-Path $projectRoot "out/package/payload/$arch"
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    $built = "out/build/package-$arch/src/windows/Release"
    Copy-Item -LiteralPath "$built/stroke_tsf.dll", "$built/stroke_setup.exe" -Destination $destination -Force
    # This file was staged by the former Win32 settings build. Remove only it.
    $legacySettings = Join-Path $destination 'stroke_settings.exe'
    if (Test-Path -LiteralPath $legacySettings) { Remove-Item -LiteralPath $legacySettings }
    # Copy the complete bundle directly, avoiding nested dictionary/dictionary on repeated builds.
    $dict = Join-Path $destination 'dictionary'
    New-Item -ItemType Directory -Force -Path $dict | Out-Null
    Copy-Item -Path 'data/generated/conway-v2.0.2-traditional/*' -Destination $dict -Recurse -Force
    Invoke-BuildTool $dictTool @('inspect', '--index', (Join-Path $dict 'dictionary.sidx'))
}
& (Join-Path $projectRoot 'apps/settings/winui/build.ps1') -Restore
$settingsPayload = Join-Path $projectRoot 'out/package/payload/settings'
New-Item -ItemType Directory -Force -Path $settingsPayload | Out-Null
Copy-Item -Path 'out/build/settings-winui/x64/Release/*' -Destination $settingsPayload -Recurse -Force
Invoke-BuildTool $Iscc @('packaging/StrokeIME.iss')
$installer = Join-Path $projectRoot 'out/release/StrokeIME-Setup-0.4.3-win64.exe'
$hash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash
Set-Content -LiteralPath "$installer.sha256" -Value "$hash  $([IO.Path]::GetFileName($installer))" -Encoding ascii
Copy-Item -LiteralPath 'packaging/quick-start.txt' -Destination 'out/release/使用說明.txt' -Force
Compress-Archive -LiteralPath $installer, "$installer.sha256", 'out/release/使用說明.txt' -DestinationPath 'out/release/StrokeIME-0.4.3-win64.zip' -Force
Write-Output $installer




