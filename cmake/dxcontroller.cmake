# DXController's own build of its scripts, DXController.u, for the app's
# data/dxcontroller (data/dxcontroller/README): the author's release zip
# that cmake/deps.lock pins by version and SHA256, fetched into
# DXL_DEPS_SRC beforehand (deps/cache/src) by Port Ex Machina's
# scripts/dx.sh deps, as SQLite's is (cmake/sqlite.cmake). The package
# alone is taken out of it and installed beside the patch; nothing else of
# the zip is used.

file(STRINGS "${DXL_ROOT}/cmake/deps.lock" dxl_dxc_line REGEX "^dxcontroller[ \t]")
if(NOT dxl_dxc_line)
    message(FATAL_ERROR "no dxcontroller in cmake/deps.lock")
endif()
string(REGEX REPLACE "[ \t]+" ";" dxl_dxc_words "${dxl_dxc_line}")
list(GET dxl_dxc_words 1 dxl_dxc_version)
list(GET dxl_dxc_words 2 dxl_dxc_file)
list(GET dxl_dxc_words 3 dxl_dxc_sha256)
set(dxl_dxc_zip "${DXL_DEPS_SRC}/${dxl_dxc_file}")
if(NOT EXISTS "${dxl_dxc_zip}")
    message(FATAL_ERROR "no ${dxl_dxc_zip} -- Port Ex Machina's scripts/dx.sh deps ${DXL_PORT} fetches it")
endif()
file(SHA256 "${dxl_dxc_zip}" dxl_dxc_got)
if(NOT dxl_dxc_got STREQUAL dxl_dxc_sha256)
    message(FATAL_ERROR "${dxl_dxc_zip} is not the one cmake/deps.lock pins (SHA256 ${dxl_dxc_sha256})")
endif()
# Taken out anew at each configure, so that the file installed is always
# the one in the zip just checked, never one left from another.
set(dxl_dxc_dir "${CMAKE_BINARY_DIR}/deps/dxcontroller-${dxl_dxc_version}")
file(REMOVE_RECURSE "${dxl_dxc_dir}")
file(MAKE_DIRECTORY "${dxl_dxc_dir}")
execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${dxl_dxc_zip}" DXController.u
                WORKING_DIRECTORY "${dxl_dxc_dir}" RESULT_VARIABLE dxl_dxc_rc)
if(NOT dxl_dxc_rc EQUAL 0 OR NOT EXISTS "${dxl_dxc_dir}/DXController.u")
    message(FATAL_ERROR "${dxl_dxc_zip} holds no DXController.u")
endif()
message(STATUS "dxl: DXController ${dxl_dxc_version}, the author's build, from ${dxl_dxc_file}")

install(FILES data/dxcontroller/README data/dxcontroller/COPYING data/dxcontroller/DeusEx-v1.6.1.dxlpatch
              "${dxl_dxc_dir}/DXController.u" DESTINATION data/dxcontroller)
