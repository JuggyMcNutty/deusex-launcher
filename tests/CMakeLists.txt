# Host-only. Nothing here needs a display.
set(DXL_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

function(dxl_test name)
    add_executable(${name} ${name}.c)
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
dxl_test(test_xdg)
dxl_test(test_gamedb)
dxl_test(test_install)
dxl_test(test_profile)
dxl_test(test_engine_args)
dxl_test(test_lastrun)

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
