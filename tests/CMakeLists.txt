# Host-only. The core has no graphics dependency, so these need no display.
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
dxl_test(test_migrate)
dxl_test(test_safemode)
dxl_test(test_renderdev)
dxl_test(test_detail)
dxl_test(test_json)
dxl_test(test_engine_settings)
dxl_test(test_renderers)
dxl_test(test_bindings)
dxl_test(test_xdg)
dxl_test(test_gamedb)
dxl_test(test_profile)
dxl_test(test_engine_args)
dxl_test(test_lastrun)

# The processes the launcher starts, with a stand-in for the engine
# (stub_engine.c) in its place.
add_executable(stub_engine stub_engine.c)
dxl_test(test_process)
target_link_libraries(test_process PRIVATE dxl_platform)
target_compile_definitions(test_process PRIVATE DXL_STUB_ENGINE="$<TARGET_FILE:stub_engine>")
add_dependencies(test_process stub_engine)

# The whole run, with stand-ins for the screens and the game.
dxl_test(test_launch)
target_link_libraries(test_launch PRIVATE dxl_launch)

# The same checks against the real game files, which this repository does not
# carry: skipped unless gamefiles/ holds an install.
dxl_test(test_gamefiles)
target_compile_definitions(test_gamefiles PRIVATE DXL_GAMEFILES="${DXL_ROOT}/gamefiles/System")
set_tests_properties(test_gamefiles PROPERTIES SKIP_RETURN_CODE 77)

# The wizard clicked through, drawn into surfaces: no display, but a font
# (skipped without one).
if(TARGET dxl_gui)
    dxl_test(test_wizard)
    target_link_libraries(test_wizard PRIVATE dxl_gui)
    set_tests_properties(test_wizard PROPERTIES SKIP_RETURN_CODE 77)
endif()
