cmake_minimum_required(VERSION 3.21)
execute_process(COMMAND "${PROBE}" "${PROVIDER}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if (result EQUAL 0 OR NOT "${output}${error}" MATCHES "FAIL ${EXPECTED_PHASE}:")
    message(FATAL_ERROR "Expected probe failure at ${EXPECTED_PHASE}, got ${result}:\n${output}${error}")
endif ()
message(STATUS "PASS negative provider check: ${output}${error}")
