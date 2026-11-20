# Windows 10 1903 or newer on x86_64, cross-built by llvm-mingw
# (toolchain.cmake): its programs need Windows' own DLLs alone, which
# scripts/check-abi.sh --windows holds them to after each build, the PE
# headers saying Windows 10. 1903: the UTF-8 code page their manifest asks
# for (win32/app.manifest), so the C library's calls take the launcher's
# UTF-8 paths as they are. SDL3, FreeType, SDL3_ttf and libxmp-lite are
# built in from cmake/deps.lock's sources.
set(DXL_PORT_WINDOWS_MIN 10.0.18362)
set(DXL_PORT_DEPS_DEFAULT vendored)
# The programs' resources: the manifest and their version; the launcher's
# icon and the manifest, named here as the scripts take them in by their
# names, which the build's scan of a script does not follow: a file changed
# is built in again (the tests' test.rc, in tests/, is not reached from here).
set(DXL_PORT_LAUNCHER_SOURCES "${CMAKE_CURRENT_LIST_DIR}/win32/deusex-launcher.rc")
set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/win32/deusex-launcher.rc" PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/win32/DXLauncher.ico;${CMAKE_CURRENT_LIST_DIR}/win32/app.manifest")
set(DXL_PORT_CLI_SOURCES "${CMAKE_CURRENT_LIST_DIR}/win32/dxl-cli.rc")
set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/win32/dxl-cli.rc" PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/win32/app.manifest")
set(DXL_PORT_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/win32/test.rc")
