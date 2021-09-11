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

function Set-XmlElementsTextValue([xml]$XmlDocument, [string]$ElementPath, [string]$TextValue, [string]$NamespaceURI = "", [string]$NodeSeparatorCharacter = '.')
{
	# Try and get the node.	
	$node = Get-XmlNode -XmlDocument $XmlDocument -NodePath $ElementPath -NamespaceURI $NamespaceURI -NodeSeparatorCharacter $NodeSeparatorCharacter
	
	# If the node already exists, update its value.
	if ($node)
	{ 
		$node.InnerText = $TextValue
	}
	# Else the node doesn't exist yet, so create it with the given value.
	else
	{
		# Create the new element with the given value.
		$elementName = $ElementPath.Substring($ElementPath.LastIndexOf($NodeSeparatorCharacter) + 1)
 		$element = $XmlDocument.CreateElement($elementName, $XmlDocument.DocumentElement.NamespaceURI)		
		$textNode = $XmlDocument.CreateTextNode($TextValue)
		$element.AppendChild($textNode) > $null
		
		# Try and get the parent node.
		$parentNodePath = $ElementPath.Substring(0, $ElementPath.LastIndexOf($NodeSeparatorCharacter))
		$parentNode = Get-XmlNode -XmlDocument $XmlDocument -NodePath $parentNodePath -NamespaceURI $NamespaceURI -NodeSeparatorCharacter $NodeSeparatorCharacter
		
		if ($parentNode)
		{
			$parentNode.AppendChild($element) > $null
		}
		else
		{
			throw "$parentNodePath does not exist in the xml."
		}
	}
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

function Update-XML-Text($File, $xpath, $value)
{
	$File = Resolve-Path $File

	[xml] $fileContents = Get-Content -Encoding UTF8 -Path $File

	if ($null -ne $value -and $value -ne "") {
		Set-XmlElementsTextValue -XmlDocument $fileContents -ElementPath $xpath -TextValue $value
	}

	$fileContents.Save($File)
}
function Get-Current-Commit-Hash ()
{
	return ([string](git log -1 --pretty=%h)).Trim()
}

function Get-VersionInfo($type, $separator)
{
	if ($type -eq "hash")
	{
		return Get-Current-Commit-Hash
	}

	$major = [string](gitversion /showvariable Major)
	$minor = [string](gitversion /showvariable Minor)
	$patch = [string](gitversion /showvariable Patch)
	$build = [string](gitversion /showvariable BuildMetaData)

	$version = "$major.$minor.$patch"

	if ($build -ne "")
	{
		$version = $version + $separator + $build
	}

	return $version
}
