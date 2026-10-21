# Windows 10 1903 or newer on x86_64, cross-built by llvm-mingw
# (toolchain.cmake): its programs need Windows' own DLLs alone, which
# scripts/check-abi.sh --windows holds them to after each build, the PE
# headers saying Windows 10. 1903: the UTF-8 code page their manifest asks
# for (win32/app.manifest), so the C library's calls take the launcher's
# UTF-8 paths as they are. SDL3, FreeType, SDL3_ttf and libxmp-lite are
# built in from cmake/deps.lock's sources.
set(DXL_PORT_WINDOWS_MIN 10.0.18362)
set(DXL_PORT_DEPS_DEFAULT vendored)
# The programs' resources: the manifest and their version.
set(DXL_PORT_LAUNCHER_SOURCES "${CMAKE_CURRENT_LIST_DIR}/win32/deusex-launcher.rc")
set(DXL_PORT_CLI_SOURCES "${CMAKE_CURRENT_LIST_DIR}/win32/dxl-cli.rc")
set(DXL_PORT_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/win32/test.rc")
