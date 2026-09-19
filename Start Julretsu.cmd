@echo off
setlocal
if exist "%~dp0release\Julretsu-0.3.0\Julretsu.exe" (
  start "" "%~dp0release\Julretsu-0.3.0\Julretsu.exe"
) else (
  echo Build and package the native app first: powershell -File "%~dp0package.ps1"
  pause
)
