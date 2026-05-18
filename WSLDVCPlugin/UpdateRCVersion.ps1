param(
    [Parameter(Mandatory = $true)][string] $separator,
    [string] $inputPath  = (Join-Path $PSScriptRoot 'WSLDVCPlugin.rc.in'),
    [string] $outputPath = (Join-Path $PSScriptRoot 'WSLDVCPlugin.rc')
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot "..\devops\version_functions.ps1")

# Compute the version once and share between Get-FileVersion / Get-NugetVersion
# so the two helpers don't each shell out to `git describe`.
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

$substitutions = [ordered]@{
    '__FILEVERSION_COMMAS__'    = $versionComma
    '__FILEVERSION_DOTS__'      = $fileVersion
    '__INFORMATIONAL_VERSION__' = $informationalVersion
}

if (-not (Test-Path -LiteralPath $inputPath))
{
    throw "Template ${inputPath} not found. The WSLDVCPlugin .rc template must be checked into the repo as WSLDVCPlugin.rc.in."
}

# Read/write via .NET file APIs with an explicit codepage-1252 encoding so the
# script behaves the same under Windows PowerShell 5.1 (which doesn't accept
# the string "windows-1252" as a -Encoding value to Get-Content/Set-Content)
# and PowerShell 7 / .NET. The encoding provider registration is a no-op on
# .NET Framework but required on .NET (Core) before non-default codepages
# resolve via Encoding.GetEncoding(int).
try {
    [System.Text.Encoding]::RegisterProvider([System.Text.CodePagesEncodingProvider]::Instance)
} catch {
    # Already registered, or running on .NET Framework where this is a no-op.
}
$encoding = [System.Text.Encoding]::GetEncoding(1252)
$content = [System.IO.File]::ReadAllText($inputPath, $encoding)

# Fail loud if the template lost a placeholder; a partially-substituted .rc
# would compile but produce wrong/empty version metadata.
$missing = @()
foreach ($key in $substitutions.Keys) {
    if (-not $content.Contains($key)) {
        $missing += $key
    }
}
if ($missing.Count -gt 0) {
    throw "${inputPath} is missing the following placeholder(s): $($missing -join ', '). Restore from git or update this script's substitutions table."
}

foreach ($entry in $substitutions.GetEnumerator()) {
    $content = $content.Replace($entry.Key, $entry.Value)
}

[System.IO.File]::WriteAllText($outputPath, $content, $encoding)
Write-Host "Generated ${outputPath}: FileVersion=$fileVersion InformationalVersion=$informationalVersion"
