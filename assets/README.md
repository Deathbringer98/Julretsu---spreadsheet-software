# Julretsu branding
User-supplied artwork, copied without changes to the original PNG pixels.

- app-icon.png: original julretsu-icon.png.
- loading-banner.png: original Jelretsu-banner-ad.png.
- app-icon.ico: Windows icon resource derived from app-icon.png, with 16, 24, 32,
  48, 64, 128, and 256 pixel PNG entries.

Run tools/Prepare-Branding.ps1 after replacing app-icon.png to regenerate the ICO.
The native app sets its window icon from the PNG; the Windows executable embeds
the ICO for Explorer/taskbar branding. macOS .app/Dock icon packaging is not yet
implemented; GLFW runtime window icons are supported on Windows/Linux.

The splash preserves the entire banner's aspect ratio and appears before workbook
initialization. On fast interactive starts it stays for at least 650 ms, displaying
Ready once initialization completes. --no-splash skips the splash. Automated smoke
and benchmark modes have no minimum splash delay.

Asset paths are resolved relative to the executable, independently of working
directory. CMake copies assets on GUI builds; package.ps1 bundles them and the
stb_image decoder license. Missing images produce a text fallback and a diagnostic;
the automated smoke check requires both images to load.

Original SHA-256 hashes:
- app-icon.png: f9078951aa59ae5483a10373e9c516b620f20a535a92eda278ce68454b4ea07b
- loading-banner.png: f6417d25ff68f3c083badb76a0b501cd190a7b8722a3aadd00498e46977150f3
