param([Parameter(Mandatory = $true)][string] $separator)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot "..\devops\version_functions.ps1")

# Compute the version once, share it between Get-FileVersion and
# Get-NugetVersion to avoid invoking `git describe` twice.
$described = Get-DescribedVersion
$fileVersion = Get-FileVersion $separator $described
$nugetVersion = Get-NugetVersion $separator $described

$sha = (& git rev-parse --short HEAD 2>&1 | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($sha))
{
    throw "git rev-parse --short HEAD failed (exit=$LASTEXITCODE): $sha"
}

$versionComma = $fileVersion.Replace(".", ",")
$informationalVersion = "$nugetVersion+sha.$sha"

$rcPath = Join-Path $PSScriptRoot 'WSLDVCPlugin.rc'
$content = Get-Content -Encoding 'windows-1252' -Path $rcPath -Raw

$substitutions = @{
    '__FILEVERSION_COMMAS__'    = $versionComma
    '__FILEVERSION_DOTS__'      = $fileVersion
    '__INFORMATIONAL_VERSION__' = $informationalVersion
}

# Idempotent: a fully-substituted .rc (no sentinels left) is treated as
# already-patched and skipped silently. A mixed state -- some sentinels
# substituted and some not -- means the file is corrupt; re-running cannot
# fix it without knowing the original values, so fail loud.
$present = 0
foreach ($key in $substitutions.Keys) {
    if ($content.Contains($key)) { $present++ }
}

if ($present -eq 0) {
    Write-Host "${rcPath} has no version placeholders (already substituted) - skipping."
    return
}
if ($present -ne $substitutions.Count) {
    throw "${rcPath} is partially substituted ($present of $($substitutions.Count) placeholders present). Restore from git and re-run."
}

foreach ($entry in $substitutions.GetEnumerator()) {
    $content = $content.Replace($entry.Key, $entry.Value)
}

Set-Content -Encoding 'windows-1252' -Path $rcPath -Value $content -NoNewline
Write-Host "Patched ${rcPath}: FileVersion=$fileVersion InformationalVersion=$informationalVersion"
