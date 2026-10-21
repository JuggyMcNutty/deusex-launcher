# The host's, and Windows' under Wine but for POSIX's own (below). Nothing
# here needs a display but test_ui_scripts, which runs on the desktop's and
# is skipped without one: the UI's other tests draw with SDL's software
# renderer on a surface.
set(DXL_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# On Windows, each test with the programs' manifest: the UTF-8 code page
# their paths need (ports/windows-x86_64/win32/test.rc).
function(dxl_test name)
    add_executable(${name} ${name}.c ${DXL_PORT_TEST_SOURCES})
    target_link_libraries(${name} PRIVATE dxl_core)
    target_include_directories(${name} PRIVATE "${CMAKE_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(${name} PRIVATE DXL_FIXTURES="${DXL_FIXTURES}")
    add_test(NAME ${name} COMMAND ${name})
endfunction()

dxl_test(test_ini)
dxl_test(test_cmdline)
dxl_test(test_policy)
dxl_test(test_argv)
dxl_test(test_config)
dxl_test(test_runtime)
dxl_test(test_localize)
dxl_test(test_json)
dxl_test(test_engine_settings)
dxl_test(test_renderers)
dxl_test(test_bindings)
dxl_test(test_gamedb)
dxl_test(test_install)
dxl_test(test_profile)
dxl_test(test_engine_args)
dxl_test(test_lastrun)
dxl_test(test_vdf)
dxl_test(test_bigscreen)
dxl_test(test_gamespy)
dxl_test(test_servers)
dxl_test(test_mods)
dxl_test(test_advice)
dxl_test(test_keys)
dxl_test(test_engine_sections)
dxl_test(test_settings)
dxl_test(test_padcurve)
dxl_test(test_padmap)
dxl_test(test_upkg)
dxl_test(test_saves)
target_compile_definitions(test_settings PRIVATE DXL_ES_DEFAULTS="${DXL_ROOT}/data/engine-settings.json.default")

find_package(Threads REQUIRED)

# The UI kit: focus and navigation, text, input from the keyboard, the
# mouse and virtual pads.
foreach(name test_ui_focus test_ui_text test_ui_input test_ui_controls)
    dxl_test(${name})
    target_link_libraries(${name} PRIVATE dxl_ui)
    target_compile_definitions(${name} PRIVATE DXL_FONTS="${DXL_ROOT}/data/fonts")
endforeach()
# The window's sound, on SDL's dummy output; the install's title music when
# gamefiles/ has one.
dxl_test(test_ui_audio)
target_link_libraries(test_ui_audio PRIVATE dxl_ui)
dxl_test(test_ui_thumbs)
target_link_libraries(test_ui_thumbs PRIVATE dxl_ui)
target_compile_definitions(test_ui_audio PRIVATE DXL_GAMEFILES="${DXL_ROOT}/gamefiles/System")

# Every page at every size and with every device, without a diagnostic
# (tools/shots.c): a test for each size, so they run side by side.
foreach(size 1280x720- 1280x800- 1920x1080- 2560x1440 3840x2160 960x600- 1920x1080big 1280x800big)
    string(REPLACE "-" "" name "${size}")
    add_test(NAME test_ui_shots_${name} COMMAND dxl-shots --only=${size})
endforeach()

# POSIX's own, not Windows': the XDG folders, the finder's Linux places,
# the desktop entry and the trash as freedesktop.org has them, the server
# browser's master and servers of its own on 127.0.0.1, the processes the
# launcher starts and the game's run with a stand-in for the engine, the
# real game files' checks and the UI's scripts, all on POSIX's calls.
if(WIN32)
    return()
endif()
dxl_test(test_xdg)
dxl_test(test_finder)
dxl_test(test_desktop)
target_link_libraries(test_desktop PRIVATE dxl_platform)
dxl_test(test_trash)
target_link_libraries(test_trash PRIVATE dxl_platform)
dxl_test(test_browser)
target_link_libraries(test_browser PRIVATE dxl_app Threads::Threads)

# The processes the launcher starts and the game's run, with a stand-in for
# the engine (stub_engine.c) in its place.
add_executable(stub_engine stub_engine.c)
foreach(name test_process test_play)
    dxl_test(${name})
    target_link_libraries(${name} PRIVATE dxl_app)
    target_compile_definitions(${name} PRIVATE DXL_STUB_ENGINE="$<TARGET_FILE:stub_engine>")
    add_dependencies(${name} stub_engine)
endforeach()

# The same checks against the real game files, which this repository does not
# carry: skipped unless gamefiles/ holds an install.
dxl_test(test_gamefiles)
target_link_libraries(test_gamefiles PRIVATE dxl_app)
target_compile_definitions(test_gamefiles PRIVATE DXL_GAMEFILES="${DXL_ROOT}/gamefiles/System"
                           DXL_STUB_ENGINE="$<TARGET_FILE:stub_engine>")
add_dependencies(test_gamefiles stub_engine)
set_tests_properties(test_gamefiles PROPERTIES SKIP_RETURN_CODE 77)

# The window through its own loop, scripts of input (tests/ui/) driving it,
# the stub engine in the engine's place: on the desktop's display, skipped
# without one or without an install in gamefiles/.
dxl_test(test_ui_scripts)
target_compile_definitions(test_ui_scripts PRIVATE
    DXL_LAUNCHER="$<TARGET_FILE:deusex-launcher>" DXL_CLI="$<TARGET_FILE:dxl-cli>"
    DXL_STUB_ENGINE="$<TARGET_FILE:stub_engine>" DXL_DATA="${DXL_ROOT}/data"
    DXL_UI_SCRIPTS="${CMAKE_CURRENT_SOURCE_DIR}/ui" DXL_GAME="${DXL_ROOT}/gamefiles")
target_link_libraries(test_ui_scripts PRIVATE Threads::Threads)
add_dependencies(test_ui_scripts deusex-launcher dxl-cli stub_engine)
set_tests_properties(test_ui_scripts PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 300)
