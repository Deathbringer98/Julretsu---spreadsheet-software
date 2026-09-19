# Branding update 0.2.1
The supplied Julretsu icon is integrated into the Windows executable's icon
resources and the GLFW window icon. The supplied banner is used as a complete,
uncropped startup splash, with startup status and an indeterminate activity bar.
Source PNGs are unchanged.

The splash renders before the initial worksheet is constructed. A short 650 ms
minimum keeps it visible on fast starts; it shows Ready after setup completes.
--no-splash bypasses it. Closing during startup exits cleanly. Graphics resources
are cleaned up before the OpenGL context is destroyed.

The packaged build is release/Julretsu-0.2.1/Julretsu.exe; Start Julretsu.cmd points
to it. Older packages and any running workbook are preserved.

Validation: native Release build, all three existing CTest targets, and the native
keyboard/mouse smoke check passed. The smoke check also required both artwork
files to decode and saved a startup screenshot, which was visually reviewed.
The Windows ICO contains seven sizes from 16 through 256 pixels. The original
PNG hashes were verified against the supplied files.

Windows behavior is verified locally. Linux window-icon/splash paths and macOS
splash paths are implemented but were not run here. A macOS .app icon is still a
future packaging task. See assets/README.md for asset updates and dependencies.
