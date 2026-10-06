set(work "${SERDE_BINARY_DIR}/installed-consumer")
set(prefix "${work}/prefix")
file(MAKE_DIRECTORY "${work}/source")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/installed_consumer/CMakeLists.txt"
    "${CMAKE_CURRENT_LIST_DIR}/header_smoke.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/module_smoke.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/field_contract.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/json_contract.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/toml_stream_contract.cpp"
    DESTINATION "${work}/source")

function(run_checked)
    execute_process(COMMAND ${ARGV}
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Installed consumer failed (${result}): ${ARGV}\n${out}\n${err}")
    endif()
endfunction()

run_checked("${CMAKE_COMMAND}" --install "${SERDE_BINARY_DIR}"
    --prefix "${prefix}" --config "${SERDE_CONFIG}")
run_checked("${CMAKE_COMMAND}" -S "${work}/source" -B "${work}/build"
    -G "${SERDE_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${SERDE_COMPILER}"
    "-DCMAKE_CXX_FLAGS=${SERDE_COMPILER_FLAGS}"
    "-DCMAKE_BUILD_TYPE=${SERDE_CONFIG}"
    "-DSERDE_EXPECT_NO_MODULES=${SERDE_EXPECT_NO_MODULES}"
    "-DCMAKE_PREFIX_PATH=${prefix}")
run_checked("${CMAKE_COMMAND}" --build "${work}/build"
    --config "${SERDE_CONFIG}" --parallel 2)
run_checked("${SERDE_CTEST_COMMAND}" --test-dir "${work}/build"
    -C "${SERDE_CONFIG}" --output-on-failure)
