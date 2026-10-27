foreach(exe IN ITEMS "${UCI}" "${CLI}")
    execute_process(COMMAND "${exe}" --version
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 5)
    string(STRIP "${output}" output)
    if(NOT result EQUAL 0 OR NOT output STREQUAL "try4 ${EXPECTED_VERSION}")
        message(FATAL_ERROR "Version check failed: ${exe}: ${output} ${error}")
    endif()
endforeach()

set(input "${CMAKE_CURRENT_BINARY_DIR}/release-smoke-input.txt")
file(WRITE "${input}" "uci\nisready\nquit\n")
execute_process(COMMAND "${UCI}" INPUT_FILE "${input}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 5)
file(REMOVE "${input}")
string(FIND "${output}" "id name try4 ${EXPECTED_VERSION}" version_at)
if(NOT result EQUAL 0 OR version_at LESS 0 OR
   NOT output MATCHES "uciok" OR NOT output MATCHES "readyok")
    message(FATAL_ERROR "UCI handshake failed: ${output} ${error}")
endif()
