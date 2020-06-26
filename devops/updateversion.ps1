param ([string] $XmlFile, [string] $xpath, [string] $name, [string] $buildSeparator = ".", [string] $type = "version")

. .\devops\version_functions.ps1

$version = Get-VersionInfo $type $buildSeparator

if ($name -eq "")
{
	Update-XML-Text $XmlFile $xpath $version
}
else
{
	Update-XML-Attribute $XmlFile $xpath $name $version
}
