param([switch]$Restore)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'Visual Studio C++ Build Tools is required.' }
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = $msbuild
$start.UseShellExecute = $false
$start.WorkingDirectory = $root
$clean = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($entry in [Environment]::GetEnvironmentVariables('Process').GetEnumerator()) { $clean[$entry.Key] = $entry.Value }
$start.Environment.Clear()
foreach ($entry in $clean.GetEnumerator()) { $start.Environment[$entry.Key] = $entry.Value }
foreach ($arg in @('apps/settings/winui/StrokeSettings.vcxproj', '/p:Configuration=Release', '/p:Platform=x64', '/m:1', '/v:minimal')) { $start.ArgumentList.Add($arg) }
if ($Restore) { $start.ArgumentList.Add('/restore'); $start.ArgumentList.Add('/p:RestoreLockedMode=true') }
$process = [Diagnostics.Process]::Start($start)
$process.WaitForExit()
if ($process.ExitCode -ne 0) { throw "WinUI build failed: $($process.ExitCode)" }

# Ship the approved app-local C++ redistributable alongside the self-contained
# Windows App SDK files. The IME DLL retains its existing static CRT build.
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath
$crt = Get-ChildItem -LiteralPath (Join-Path $vsRoot 'VC/Redist/MSVC') -Directory |
    Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } | Sort-Object { [version]$_.Name } -Descending |
    ForEach-Object { Get-ChildItem -Path (Join-Path $_.FullName 'x64/Microsoft.VC*.CRT') -Directory } | Select-Object -First 1
if (-not $crt) { throw 'The x64 Visual C++ redistributable directory is missing.' }
$output = Join-Path $root 'out/build/settings-winui/x64/Release'
Copy-Item -Path (Join-Path $crt.FullName '*.dll') -Destination $output -Force

# Preserve package license/notice texts, including build-time dependencies.
$licenses = Join-Path $output 'licenses'
$crtNotices = Join-Path $licenses 'Microsoft.VC.CRT'
New-Item -ItemType Directory -Force -Path $crtNotices | Out-Null
Get-ChildItem -Path (Join-Path $vsRoot 'Licenses/*/Redist.txt'), (Join-Path $vsRoot 'Licenses/*/ThirdPartyNotices.txt') -File |
    Copy-Item -Destination $crtNotices -Force
$packages = Join-Path $root 'out/dependencies/nuget'
Get-ChildItem -LiteralPath $packages -Directory | ForEach-Object {
    $package = $_
    Get-ChildItem -LiteralPath $package.FullName -Directory | ForEach-Object {
        $version = $_
        $destination = Join-Path $licenses ($package.Name + '/' + $version.Name)
        New-Item -ItemType Directory -Force -Path $destination | Out-Null
        Get-ChildItem -LiteralPath $version.FullName -File | Where-Object { $_.Name -match 'license|notice|copying|\.nuspec$' } |
            Copy-Item -Destination $destination -Force
    }
}
