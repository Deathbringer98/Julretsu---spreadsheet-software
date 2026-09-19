param([string]$AppDirectory=$PSScriptRoot)
$ErrorActionPreference='Stop'
$executable=Join-Path $AppDirectory 'Julretsu.exe'
$icon=Join-Path $AppDirectory 'assets\app-icon.ico'
if (-not (Test-Path -LiteralPath $executable) -or -not (Test-Path -LiteralPath $icon)) { throw 'Keep this script beside Julretsu.exe and its assets folder.' }
$executable=(Resolve-Path -LiteralPath $executable).Path
$icon=(Resolve-Path -LiteralPath $icon).Path
# Per-user registration; no administrator privileges or UserChoice override.
$classes='Registry::HKEY_CURRENT_USER\Software\Classes'
$type=Join-Path $classes 'Julretsu.Workbook'
New-Item -Path $type -Force | Out-Null
Set-Item -LiteralPath $type -Value 'Julretsu Workbook'
New-Item -Path "$type\DefaultIcon" -Force | Out-Null
Set-Item -LiteralPath "$type\DefaultIcon" -Value ('"'+$icon+'",0')
New-Item -Path "$type\shell\open\command" -Force | Out-Null
Set-Item -LiteralPath "$type\shell\open\command" -Value ('"'+$executable+'" --open "%1"')
$extension=Join-Path $classes '.julretsu'
New-Item -Path $extension -Force | Out-Null
Set-Item -LiteralPath $extension -Value 'Julretsu.Workbook'
New-Item -Path "$extension\OpenWithProgids" -Force | Out-Null
New-ItemProperty -LiteralPath "$extension\OpenWithProgids" -Name 'Julretsu.Workbook' -PropertyType String -Value '' -Force | Out-Null
if (-not ('Julretsu.AssociationRefresh' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace Julretsu { public static class AssociationRefresh {
    [DllImport("shell32.dll")] public static extern void SHChangeNotify(uint eventId,uint flags,IntPtr item1,IntPtr item2);
} }
'@
}
[Julretsu.AssociationRefresh]::SHChangeNotify(0x08000000,0,[IntPtr]::Zero,[IntPtr]::Zero)
Write-Output 'Registered .julretsu files with the original Julretsu icon and workbook opener.'
