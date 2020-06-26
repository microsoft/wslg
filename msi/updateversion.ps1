param ([string] $XmlFile = $null )

function Get-XmlNamespaceManager([xml]$XmlDocument, [string]$NamespaceURI = "")
{
    # If a Namespace URI was not given, use the Xml document's default namespace.
	if ([string]::IsNullOrEmpty($NamespaceURI)) { $NamespaceURI = $XmlDocument.DocumentElement.NamespaceURI }	
	
	# In order for SelectSingleNode() to actually work, we need to use the fully qualified node path along with an Xml Namespace Manager, so set them up.
	[System.Xml.XmlNamespaceManager]$xmlNsManager = New-Object System.Xml.XmlNamespaceManager($XmlDocument.NameTable)
	$xmlNsManager.AddNamespace("ns", $NamespaceURI)
    return ,$xmlNsManager		# Need to put the comma before the variable name so that PowerShell doesn't convert it into an Object[].
}

function Get-FullyQualifiedXmlNodePath([string]$NodePath, [string]$NodeSeparatorCharacter = '.')
{
    return "/ns:$($NodePath.Replace($('.'), '/ns:'))"
}

function Get-XmlNode([xml]$XmlDocument, [string]$NodePath, [string]$NamespaceURI = "", [string]$NodeSeparatorCharacter = '.')
{
	$xmlNsManager = Get-XmlNamespaceManager -XmlDocument $XmlDocument -NamespaceURI $NamespaceURI
	[string]$fullyQualifiedNodePath = Get-FullyQualifiedXmlNodePath -NodePath $NodePath -NodeSeparatorCharacter $NodeSeparatorCharacter
	
	# Try and get the node, then return it. Returns $null if the node was not found.
	$node = $XmlDocument.SelectSingleNode($fullyQualifiedNodePath, $xmlNsManager)
	return $node
}

function Set-XmlAttributeValue([xml]$XmlDocument, [string]$ElementPath, [string]$AttributeName, [string]$AttributeValue, [string]$NamespaceURI = "", [string]$NodeSeparatorCharacter = '.')
{
	# Try and get the node.	
	$node = Get-XmlNode -XmlDocument $XmlDocument -NodePath $ElementPath -NamespaceURI $NamespaceURI -NodeSeparatorCharacter $NodeSeparatorCharacter
	$node.SetAttribute($AttributeName, $AttributeValue)
}

function Update-XML-Attribute($File, $xpath, $name, $value) 
{
		$File = Resolve-Path $File
	
		[xml] $fileContents = Get-Content -Encoding UTF8 -Path $File
	
		if ($null -ne $value -and $value -ne "") {
			Set-XmlAttributeValue -XmlDocument $fileContents -ElementPath $xpath -AttributeName $name -AttributeValue $value
		}
		$fileContents.Save($File)
}

$major = [string](gitversion /showvariable Major)
$minor = [string](gitversion /showvariable Minor)
$patch = [string](gitversion /showvariable Patch)
$build = [string](gitversion /showvariable BuildMetaData)

if ($build -eq "")
{
    $build = "0"
}

if ($XmlFile -ne $null)
{
    $version = "$major.$minor.$patch.$build";
    Update-XML-Attribute $XmlFile "Wix.Product" "Version" $version
}
