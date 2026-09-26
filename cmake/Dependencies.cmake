# Dependency recipes. Including this file does not
# fetch, configure, or build any third-party dependency.
include(FetchContent)
FetchContent_Declare(stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG f5befd2d29e66809cd1110a152e375a7f1981f06)
FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 7b6aead9fb88b3623e3b3725ebb42670cbe4c579)
FetchContent_Declare(sol2
  GIT_REPOSITORY https://github.com/ThePhD/sol2.git
  GIT_TAG 9190880c593dfb018ccf5cc9729ab87739709862
  SOURCE_SUBDIR julretsu-header-only)
FetchContent_Declare(lua
  URL https://www.lua.org/ftp/lua-5.4.9.tar.gz
  URL_HASH SHA256=2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6)
# Recipes run only when the corresponding integration is enabled.
function(julretsu_prepare_gui_dependencies)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(glfw imgui stb)
  set(stb_SOURCE_DIR "${stb_SOURCE_DIR}" PARENT_SCOPE)
  find_package(OpenGL REQUIRED)
  add_library(julretsu_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
  target_include_directories(julretsu_imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_link_libraries(julretsu_imgui PUBLIC glfw OpenGL::GL)
  if(WIN32)
    # Places the Korean and Japanese input method window at the text cursor (off by default outside MSVC).
    target_compile_definitions(julretsu_imgui PRIVATE IMGUI_ENABLE_WIN32_DEFAULT_IME_FUNCTIONS)
    target_link_libraries(julretsu_imgui PUBLIC imm32)
  endif()
endfunction()
function(julretsu_prepare_lua_dependencies)
  set(SOL2_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  set(SOL2_TESTS OFF CACHE BOOL "" FORCE)
  set(SOL2_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(lua sol2)
  # sol2 is header-only. Its newer upstream CMake requires 3.26; our
  # interface target keeps this project compatible with CMake 3.20.
  add_library(julretsu_sol2 INTERFACE)
  target_include_directories(julretsu_sol2 SYSTEM INTERFACE ${sol2_SOURCE_DIR}/include)
  set(lua_src ${lua_SOURCE_DIR}/src)
  add_library(julretsu_lua_runtime STATIC
    ${lua_src}/lapi.c ${lua_src}/lauxlib.c ${lua_src}/lbaselib.c
    ${lua_src}/lcode.c ${lua_src}/lcorolib.c ${lua_src}/lctype.c
    ${lua_src}/ldblib.c ${lua_src}/ldebug.c ${lua_src}/ldo.c
    ${lua_src}/ldump.c ${lua_src}/lfunc.c ${lua_src}/lgc.c
    ${lua_src}/linit.c ${lua_src}/liolib.c ${lua_src}/llex.c
    ${lua_src}/lmathlib.c ${lua_src}/lmem.c ${lua_src}/loadlib.c
    ${lua_src}/lobject.c ${lua_src}/lopcodes.c ${lua_src}/loslib.c
    ${lua_src}/lparser.c ${lua_src}/lstate.c ${lua_src}/lstring.c
    ${lua_src}/lstrlib.c ${lua_src}/ltable.c ${lua_src}/ltablib.c
    ${lua_src}/ltm.c ${lua_src}/lundump.c ${lua_src}/lutf8lib.c
    ${lua_src}/lvm.c ${lua_src}/lzio.c)
  target_include_directories(julretsu_lua_runtime PUBLIC ${lua_src})
  if(APPLE)
    target_compile_definitions(julretsu_lua_runtime PRIVATE LUA_USE_MACOSX)
  elseif(UNIX)
    target_compile_definitions(julretsu_lua_runtime PRIVATE LUA_USE_LINUX)
  endif()
  if(UNIX)
    target_link_libraries(julretsu_lua_runtime PUBLIC m ${CMAKE_DL_LIBS})
  endif()
endfunction()

FetchContent_Declare(miniz GIT_REPOSITORY https://github.com/richgel999/miniz.git GIT_TAG 77d0dce8627735138c51770d1799a1ef48f2117d SOURCE_SUBDIR julretsu-source-only)
FetchContent_Declare(pugixml GIT_REPOSITORY https://github.com/zeux/pugixml.git GIT_TAG ee86beb30e4973f5feffe3ce63bfa4fbadf72f38 SOURCE_SUBDIR julretsu-source-only)
function(julretsu_prepare_xlsx)
  FetchContent_MakeAvailable(miniz pugixml)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/miniz_export.h "#pragma once
#define MINIZ_EXPORT
")
  add_library(julretsu_zip STATIC ${miniz_SOURCE_DIR}/miniz.c ${miniz_SOURCE_DIR}/miniz_zip.c ${miniz_SOURCE_DIR}/miniz_tdef.c ${miniz_SOURCE_DIR}/miniz_tinfl.c)
  target_include_directories(julretsu_zip SYSTEM PUBLIC ${miniz_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
  add_library(julretsu_xml STATIC ${pugixml_SOURCE_DIR}/src/pugixml.cpp)
  target_include_directories(julretsu_xml SYSTEM PUBLIC ${pugixml_SOURCE_DIR}/src)
endfunction()
