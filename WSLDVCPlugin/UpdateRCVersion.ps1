param([Parameter(Mandatory = $true)][string] $separator)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot "..\devops\version_functions.ps1")

$fileVersion = Get-FileVersion $separator
$nugetVersion = Get-NugetVersion $separator
$sha = (git rev-parse --short HEAD).Trim()

$versionComma = $fileVersion.Replace(".", ",")
$informationalVersion = "$nugetVersion+sha.$sha"

$rcPath = Join-Path $PSScriptRoot 'WSLDVCPlugin.rc'
$content = Get-Content -Encoding 'windows-1252' -Path $rcPath -Raw

$substitutions = @{
    '__FILEVERSION_COMMAS__'  = $versionComma
    '__FILEVERSION_DOTS__'    = $fileVersion
    '__INFORMATIONAL_VERSION__' = $informationalVersion
}

foreach ($entry in $substitutions.GetEnumerator()) {
    if (-not $content.Contains($entry.Key)) {
        throw "Placeholder '$($entry.Key)' not found in $rcPath. Was the file edited or already substituted?"
    }
    $content = $content.Replace($entry.Key, $entry.Value)
}

Set-Content -Encoding 'windows-1252' -Path $rcPath -Value $content -NoNewline
Write-Host "Patched ${rcPath}: FileVersion=$fileVersion InformationalVersion=$informationalVersion"
