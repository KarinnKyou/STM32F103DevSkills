# install.ps1 — copy the skills in this package into a project's .agents\skills
#
#   .\install.ps1                          # install into the folder that contains this package
#   .\install.ps1 -Target 'D:\myproject'   # install into another project
#   .\install.ps1 -List                    # show what would be installed
#
# Existing folders with the same name are replaced, so this package stays the
# single source of truth. Nothing outside <Target>\.agents\skills is touched.

param(
    [string]$Target = (Split-Path -Parent $PSScriptRoot),
    [switch]$List
)

$ErrorActionPreference = 'Stop'
$src = Join-Path $PSScriptRoot 'skills'
if (-not (Test-Path $src)) { throw "skills/ not found next to this script" }

$skills = Get-ChildItem $src -Directory
if (-not $skills) { throw "no skill directories under $src" }

$dest = Join-Path $Target '.agents\skills'

if ($List) {
    "package : $PSScriptRoot"
    "target  : $dest"
    $skills | ForEach-Object { "  $($_.Name)" }
    return
}

New-Item -ItemType Directory -Path $dest -Force | Out-Null

foreach ($s in $skills) {
    $to = Join-Path $dest $s.Name
    if (Test-Path $to) { Remove-Item $to -Recurse -Force }
    Copy-Item $s.FullName $dest -Recurse -Force
    "installed $($s.Name)"
}

"done -> $dest"
