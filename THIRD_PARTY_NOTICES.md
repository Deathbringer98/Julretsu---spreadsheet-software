# Third-party notices

Julretsu project code is original and proprietary (all rights reserved); see LICENSE.
The components below keep their own licenses; the Julretsu license does not cover them.
The dependency-free headless core contains no copied third-party library source.

Product inspiration: https://github.com/genspark-ai/genoffice .
No code, branding assets, or UI assets were copied from GenOffice.

The native application uses:
- Dear ImGui v1.91.9b: MIT, https://github.com/ocornut/imgui/blob/v1.91.9b/LICENSE.txt
- GLFW 3.4: zlib/libpng, https://github.com/glfw/glfw/blob/3.4/LICENSE.md
- sol2 v3.5.0: MIT, https://github.com/ThePhD/sol2/blob/v3.5.0/LICENSE.txt
- Lua 5.4.9: MIT, https://www.lua.org/license.html
- stb_image: MIT/public-domain dual license, pinned commit
  2c980bb59875b0d32144a71867fbdebb2f77cd20,
  https://github.com/nothings/stb/blob/2c980bb59875b0d32144a71867fbdebb2f77cd20/LICENSE

App icon and loading-banner artwork were supplied by the user for Julretsu.
Original PNGs are preserved in assets; the Windows ICO is a format conversion.

The portable Windows package includes exact dependency copyright/license notices
in its licenses directory. Lua's full notice is preserved in the copied lua.h.
Its LLVM runtime DLLs use the Apache 2.0 license with LLVM exceptions; the
package includes the toolchain license and MinGW runtime notices.

The Windows Segoe UI font is loaded from the existing OS font installation and
is not redistributed. Other platforms use ImGui's bundled default font.

The local .tools directory contains optional downloaded development tools with
their upstream licenses. .tools, .deps, build outputs, and the portable package
are ignored by Git. They are not relabeled as Julretsu-owned source.
