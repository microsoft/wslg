# Parses `git describe --tags --match *.*.* --abbrev=1` output into a
# structured object. The describe output is one of:
#   "v1.0.77"           - HEAD is exactly on the tag (revision = 0)
#   "v1.0.77-3-gabc123" - HEAD is 3 commits past the tag (revision = 3)
# Tags may or may not be prefixed with 'v'. Throws if no Major.Minor.Patch tag
# is reachable.
#
# This is the PowerShell version. devops/get-nuget-version.sh is the bash
# equivalent used by the Linux Dockerfile / build-and-export.sh. Both
# implementations MUST stay in sync; if you touch one, update the other.
function Get-DescribedVersion()
{
	# --match '*.*.*' --exclude '*.*.*.*' picks the most recent tag whose
	# name looks like Major.Minor.Patch (3 numeric segments). Excluding
	# 4-segment tags lets us cosmetically tag built releases like
	# 'v1.0.73.1' without confusing the version derivation -- the
	# describe call will skip over them to the underlying 'v1.0.73' tag,
	# and the patch-count we compute will be relative to that.
	#
	# `2>&1 | Out-String` is used instead of `2>&1` alone: PowerShell turns
	# native stderr into ErrorRecord objects, which then mix with strings in
	# the output array and break the `.Trim().Split('-')` chain. Out-String
	# collapses everything to a single string for safe parsing on success
	# (and a clean message in the failure path below).
	$output = (& git describe --tags --match '*.*.*' --exclude '*.*.*.*' --abbrev=1 2>&1 | Out-String)
	if ($LASTEXITCODE -ne 0)
	{
		throw "git describe failed (exit=$LASTEXITCODE): $output. Make sure tags are available -- a shallow clone usually isn't enough."
	}

	$parts = $output.Trim().Split('-')
	$base = $parts[0] -replace '^v', ''
	if ($base -notmatch '^\d+\.\d+\.\d+$')
	{
		throw "git describe output '$output' does not start with a Major.Minor.Patch tag (parsed base: '$base')."
	}
	$semver = $base.Split('.')

	# Validate revision explicitly (mirrors get-nuget-version.sh). The
	# implicit [int] cast would throw a generic "Cannot convert" if a tag
	# like 'v1.0.0-rc1' ever sneaks in; the explicit check gives a useful
	# message naming the actual describe output.
	if ($parts.Length -lt 2)
	{
		$revision = 0
	}
	else
	{
		$rev = $parts[1]
		if ($rev -notmatch '^\d+$')
		{
			throw "Parsed revision '$rev' from describe output '$output' is not numeric."
		}
		$revision = [int]$rev
	}

	return [pscustomobject]@{
		BaseVersion = $base
		Major       = [int]$semver[0]
		Minor       = [int]$semver[1]
		Patch       = [int]$semver[2]
		Revision    = $revision
	}
}

# Validates that an object passed to Get-NugetVersion / Get-FileVersion as a
# pre-computed describe result has all the expected members with sensible
# types. Returns the object unchanged on success; throws a clear error on the
# first missing/wrong-typed property. This catches the case where a caller
# passes a hashtable (which PowerShell will silently coerce to PSObject and
# then yield $null .Major / .Revision, producing meaningless "0.0.1-Beta"
# versions).
function Assert-DescribedVersion($v)
{
	$required = @{
		BaseVersion = 'string'
		Major       = 'int'
		Minor       = 'int'
		Patch       = 'int'
		Revision    = 'int'
	}
	foreach ($name in $required.Keys)
	{
		if (-not ($v.PSObject.Properties.Name -contains $name))
		{
			throw "describedVersion is missing required property '$name' (got: $($v.PSObject.Properties.Name -join ', '))."
		}
		$value = $v.$name
		if ($null -eq $value)
		{
			throw "describedVersion.$name is `$null."
		}
		$expected = $required[$name]
		if ($expected -eq 'string' -and -not ($value -is [string]))
		{
			throw "describedVersion.$name must be a string, got [$($value.GetType().FullName)]."
		}
		if ($expected -eq 'int' -and -not ($value -is [int]))
		{
			throw "describedVersion.$name must be an int, got [$($value.GetType().FullName)]."
		}
	}
	return $v
}

# Returns the NuGet package version. The $separator argument selects the
# convention to apply to off-tag builds:
#   "."      -> release/* convention: keep the base tag, append ".$revision"
#               (e.g. v1.0.73 + 1 commit  ->  "1.0.73.1")
#   anything -> main/feature convention: bump Patch, append "$separator$revision"
#               (e.g. v1.0.77 + 3 commits, "-Beta"  ->  "1.0.78-Beta3")
# When HEAD is exactly on a tag the base version is returned unchanged
# regardless of separator. $separator is required so that callers (notably CI
# pipelines) cannot silently default to the wrong versioning convention.
function Get-NugetVersion
{
	param(
		[Parameter(Mandatory = $true)][string]$separator,
		# Optional: caller can pass a pre-computed Get-DescribedVersion result
		# to avoid re-invoking `git describe` when both Get-NugetVersion and
		# Get-FileVersion are needed (e.g. UpdateRCVersion.ps1). Typed as
		# [pscustomobject] and shape-checked below so that a caller passing
		# the wrong shape (a hashtable, an empty object, $null wrapped, ...)
		# fails loudly here instead of silently producing "0.0.1-Beta".
		[Parameter(Mandatory = $false)][pscustomobject]$describedVersion = $null
	)
	$v = if ($null -ne $describedVersion) { Assert-DescribedVersion $describedVersion } else { Get-DescribedVersion }
	if ($v.Revision -eq 0) { return $v.BaseVersion }

	if ($separator -eq ".")
	{
		return "$($v.BaseVersion).$($v.Revision)"
	}

	$bumped = "{0}.{1}.{2}" -f $v.Major, $v.Minor, ($v.Patch + 1)
	return "$bumped$separator$($v.Revision)"
}

# Returns the 4-part Major.Minor.Patch.0 file version stamped into the
# WSLDVCPlugin DLL resource. The 4th part is intentionally fixed at 0 -- the
# DLL's release identity is the Major.Minor.Patch tag it was built from. Patch
# is bumped on off-tag main/feature builds (separator != "."), preserved on
# release/* and on-tag builds. $separator is required for the same reason as
# Get-NugetVersion.
function Get-FileVersion
{
	param(
		[Parameter(Mandatory = $true)][string]$separator,
		[Parameter(Mandatory = $false)][pscustomobject]$describedVersion = $null
	)
	$v = if ($null -ne $describedVersion) { Assert-DescribedVersion $describedVersion } else { Get-DescribedVersion }
	if ($separator -eq "." -or $v.Revision -eq 0)
	{
		return "$($v.BaseVersion).0"
	}

	return ("{0}.{1}.{2}.0" -f $v.Major, $v.Minor, ($v.Patch + 1))
}
