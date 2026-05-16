param([Parameter(Mandatory = $true)][string] $separator)

. (Join-Path $PSScriptRoot "..\devops\version_functions.ps1")

$fileVersion = Get-FileVersion $separator
$nugetVersion = Get-NugetVersion $separator
$sha = (git rev-parse --short HEAD).Trim()

$versionComma = $fileVersion.Replace(".", ",")
$informationalVersion = "$nugetVersion+sha.$sha"

$content = (Get-Content -Encoding "windows-1252" -Path ".\WSLDVCPlugin.rc")
$content = $content.Replace("__FILEVERSION_COMMAS__", $versionComma)
$content = $content.Replace("__FILEVERSION_DOTS__", $fileVersion)
$content = $content.Replace("__INFORMATIONAL_VERSION__", $informationalVersion)

Set-Content -Encoding "windows-1252" -Path ".\WSLDVCPlugin.rc" -Value $content
