param([Parameter(Mandatory = $true)][string] $separator)

. (Join-Path $PSScriptRoot "..\devops\version_functions.ps1")

$fileVersion = Get-FileVersion $separator
$nugetVersion = Get-NugetVersion $separator
$sha = Get-Current-Commit-Hash

$versionComma = $fileVersion.Replace(".", ",")
$informationalVersion = "$nugetVersion+sha.$sha"

$content = (Get-Content -Encoding "windows-1252" -Path ".\WSLDVCPlugin.rc")
$content = $content.Replace("1,0,0,1", $versionComma)
$content = $content.Replace("1.0.0.1", $fileVersion)
$content = $content.Replace("InformationalVersion", $informationalVersion)

Set-Content -Encoding "windows-1252" -Path ".\WSLDVCPlugin.rc" -Value $content
