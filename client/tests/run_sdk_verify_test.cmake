if(NOT DEFINED VERIFIER OR NOT EXISTS "${VERIFIER}")
    message(FATAL_ERROR "Native SDK verifier is missing: ${VERIFIER}")
endif()
if(NOT DEFINED FAKE_SDK OR NOT EXISTS "${FAKE_SDK}")
    message(FATAL_ERROR "Fake SDK is missing: ${FAKE_SDK}")
endif()

# This verifier check intentionally stops at initialization; it must not authorize a fake Run.
set(test_root "${CMAKE_CURRENT_BINARY_DIR}/sdk-verify-test")
file(MAKE_DIRECTORY "${test_root}/output")
file(WRITE "${test_root}/init.ini" "test")
file(WRITE "${test_root}/PTScan.par" "test")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "PATH=C:/msys64/ucrt64/bin;$ENV{PATH}"
        "${VERIFIER}"
        --sdk "${FAKE_SDK}"
        --init "${test_root}/init.ini"
        --par "${test_root}/PTScan.par"
        --output "${test_root}/output"
    RESULT_VARIABLE verifier_result
    OUTPUT_VARIABLE verifier_output
    ERROR_VARIABLE verifier_error
)

if(NOT verifier_result EQUAL 0)
    message(FATAL_ERROR
        "Native SDK verifier failed with ${verifier_result}\n"
        "stdout:\n${verifier_output}\n"
        "stderr:\n${verifier_error}")
endif()
# End native verifier initialization check.

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "PATH=C:/msys64/ucrt64/bin;$ENV{PATH}"
        "${VERIFIER}"
        --sdk "${FAKE_SDK}"
        --init "${test_root}/init.ini"
        --par "${test_root}/PTScan.par"
        --output "${test_root}/output"
        --scan
    RESULT_VARIABLE scan_result
    OUTPUT_VARIABLE scan_output
    ERROR_VARIABLE scan_error
)

if(scan_result EQUAL 0 OR NOT scan_error MATCHES "Unknown option 'scan'")
    message(FATAL_ERROR
        "The verifier must reject the removed unsafe --scan option\n"
        "result: ${scan_result}\nstdout:\n${scan_output}\nstderr:\n${scan_error}")
endif()
