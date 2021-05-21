
$version = [string](gitversion /showvariable AssemblySemFileVer)
$versionComma = $version.Replace(".", ",")
$informationalVersion = [string](gitversion /showvariable InformationalVersion)

$content = (Get-Content -Encoding "windows-1252" -Path ".\WSLDVCPlugin.rc")
$content = $content.Replace("1,0,0,1", $versionComma);
$content = $content.Replace("1.0.0.1", $version);
$content = $content.Replace("InformationalVersion", $InformationalVersion);

Set-Content -Encoding "windows-1252" -Path ".\WSLDVCPlugin.rc" -Value $content