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

# Device profiles: the generic one and the lookup rules, then each port's own
# target.c checked against its port (its id, and CPU modes its run-game hooks
# handle).
add_executable(test_target test_target.c
    "${DXL_ROOT}/src/platform/target.c" "${DXL_ROOT}/src/platform/target_default.c")
target_link_libraries(test_target PRIVATE dxl_core)
target_include_directories(test_target PRIVATE "${DXL_ROOT}/src" "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME test_target COMMAND test_target)

file(GLOB DXL_PORT_TARGETS "${DXL_ROOT}/ports/*/target.c")
foreach(src ${DXL_PORT_TARGETS})
    get_filename_component(dir "${src}" DIRECTORY)
    get_filename_component(id "${dir}" NAME)
    string(MAKE_C_IDENTIFIER "test_target_${id}" name)
    add_executable(${name} test_target_port.c "${src}" "${DXL_ROOT}/src/platform/target.c")
    target_link_libraries(${name} PRIVATE dxl_core)
    target_include_directories(${name} PRIVATE "${DXL_ROOT}/src" "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(${name} PRIVATE DXL_PORT_ID="${id}" DXL_PORT_DIR="${dir}")
    add_test(NAME ${name} COMMAND ${name})
endforeach()
