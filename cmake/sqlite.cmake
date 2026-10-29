# SQLite, which the finder reads Lutris' games database with
# (core/finder_places.c): always built here, never the system's, from the
# amalgamation cmake/deps.lock pins, fetched into DXL_DEPS_SRC beforehand
# (deps/cache/src), as the static library dxl_sqlite. Third-party code: its
# own warnings are not shown (-w). Read only, no extensions loaded.

file(STRINGS "${DXL_ROOT}/cmake/deps.lock" dxl_sqlite_line REGEX "^sqlite[ \t]")
if(NOT dxl_sqlite_line)
    message(FATAL_ERROR "no sqlite in cmake/deps.lock")
endif()
string(REGEX REPLACE "[ \t]+" ";" dxl_sqlite_words "${dxl_sqlite_line}")
list(GET dxl_sqlite_words 1 dxl_sqlite_version)
list(GET dxl_sqlite_words 2 dxl_sqlite_file)
list(GET dxl_sqlite_words 3 dxl_sqlite_sha256)
set(dxl_sqlite_zip "${DXL_DEPS_SRC}/${dxl_sqlite_file}")
if(NOT EXISTS "${dxl_sqlite_zip}")
    message(FATAL_ERROR "no ${dxl_sqlite_zip} -- Port Ex Machina's scripts/dx.sh deps ${DXL_PORT} fetches it")
endif()
file(SHA256 "${dxl_sqlite_zip}" dxl_sqlite_got)
if(NOT dxl_sqlite_got STREQUAL dxl_sqlite_sha256)
    message(FATAL_ERROR "${dxl_sqlite_zip} is not the one cmake/deps.lock pins (SHA256 ${dxl_sqlite_sha256})")
endif()
# Unpacked once a version, beside the other libraries' sources.
string(REGEX REPLACE "\\.zip$" "" dxl_sqlite_stem "${dxl_sqlite_file}")
set(dxl_sqlite_src "${CMAKE_BINARY_DIR}/deps/dep_sqlite-prefix/src")
if(NOT EXISTS "${dxl_sqlite_src}/${dxl_sqlite_stem}/sqlite3.c")
    file(MAKE_DIRECTORY "${dxl_sqlite_src}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${dxl_sqlite_zip}"
                    WORKING_DIRECTORY "${dxl_sqlite_src}" RESULT_VARIABLE dxl_sqlite_rc)
    if(NOT dxl_sqlite_rc EQUAL 0 OR NOT EXISTS "${dxl_sqlite_src}/${dxl_sqlite_stem}/sqlite3.c")
        message(FATAL_ERROR "${dxl_sqlite_zip} did not unpack to ${dxl_sqlite_stem}/sqlite3.c")
    endif()
endif()

add_library(dxl_sqlite STATIC "${dxl_sqlite_src}/${dxl_sqlite_stem}/sqlite3.c")
target_include_directories(dxl_sqlite SYSTEM PUBLIC "${dxl_sqlite_src}/${dxl_sqlite_stem}")
target_compile_definitions(dxl_sqlite PRIVATE SQLITE_OMIT_LOAD_EXTENSION SQLITE_OMIT_DEPRECATED
                           SQLITE_OMIT_SHARED_CACHE SQLITE_DQS=0 SQLITE_DEFAULT_MEMSTATUS=0)
target_compile_options(dxl_sqlite PRIVATE -w)
find_package(Threads REQUIRED)
target_link_libraries(dxl_sqlite PUBLIC Threads::Threads m)
message(STATUS "dxl: SQLite ${dxl_sqlite_version}, built here from cmake/deps.lock's amalgamation")
