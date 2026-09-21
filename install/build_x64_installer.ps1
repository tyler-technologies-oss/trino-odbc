<#
.SYNOPSIS
  Builds the 64-bit TrinoODBC MSI installer.
.PARAMETER Version
  The MAJOR.MINOR.PATCH version to stamp onto the package. The release
  workflow passes the version from the tag that triggered it. The
  default matches the placeholder CMake uses, so a locally built
  installer is easy to tell apart from a released one.
#>
param(
  [ValidatePattern('^\d+\.\d+\.\d+$')]
  [string]$Version = "0.0.0"
)

$OriginalDirectory = Get-Location
try {
  Set-Location -Path $PSScriptRoot
  wix build -ext WixToolset.UI.wixext -arch x64 -d Version=$Version .\TrinoODBC_x64.wxs
}
finally {
  Set-Location -Path $OriginalDirectory
}
