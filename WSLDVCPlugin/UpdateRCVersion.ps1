
$version = [string](gitversion /showvariable AssemblySemFileVer)
$versionComma = $version.Replace(".", ",")
$informationalVersion = [string](gitversion /showvariable InformationalVersion)

# TODO: Add -Encoding "windows-1252" ? it seems is only supported on newer versions of PS
$content = (Get-Content -Path ".\WSLDVCPlugin.rc")
$content = $content.Replace("1,0,0,1", $versionComma);
$content = $content.Replace("1.0.0.1", $version);
$content = $content.Replace("InformationalVersion", $InformationalVersion);

Set-Content -Path ".\WSLDVCPlugin.rc" -Value $content