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
dxl_test(test_json)
dxl_test(test_engine_settings)
dxl_test(test_renderers)
dxl_test(test_bindings)

# The same checks against the real game files, which this repository does not
# carry: skipped unless gamefiles/ holds an install.
dxl_test(test_gamefiles)
target_compile_definitions(test_gamefiles PRIVATE DXL_GAMEFILES="${DXL_ROOT}/gamefiles/System")
set_tests_properties(test_gamefiles PROPERTIES SKIP_RETURN_CODE 77)

# The device profile.
add_executable(test_target test_target.c "${DXL_ROOT}/src/platform/target_default.c")
target_link_libraries(test_target PRIVATE dxl_core)
target_include_directories(test_target PRIVATE "${DXL_ROOT}/src" "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME test_target COMMAND test_target)
