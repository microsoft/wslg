. .\devops\version_functions.ps1

$version = Get-VersionInfo "version" "-beta"

Write-Output $version
