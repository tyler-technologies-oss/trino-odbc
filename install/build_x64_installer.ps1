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
  # The version goes in the filename as well as inside the package.
  # Clients download these installers and keep them, so the file has
  # to stay identifiable once it is out of the release page's context.
  wix build -ext WixToolset.UI.wixext -arch x64 -d Version=$Version `
            -o ".\TrinoODBC_x64_$Version.msi" .\TrinoODBC_x64.wxs
}
finally {
  Set-Location -Path $OriginalDirectory
}
