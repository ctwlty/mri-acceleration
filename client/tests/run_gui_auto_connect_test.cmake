if(NOT DEFINED GUI OR NOT EXISTS "${GUI}")
    message(FATAL_ERROR "Qt GUI executable is missing: ${GUI}")
endif()
if(NOT DEFINED FAKE_SDK OR NOT EXISTS "${FAKE_SDK}")
    message(FATAL_ERROR "Fake SDK is missing: ${FAKE_SDK}")
endif()

set(test_root "${CMAKE_CURRENT_BINARY_DIR}/gui-auto-connect-test")
set(app_root "${test_root}/app")
set(runtime_root "${app_root}/mri-runtime")
set(call_log "${test_root}/calls.log")
file(MAKE_DIRECTORY "${runtime_root}/hw_cfg" "${runtime_root}/profiles")
file(REMOVE "${call_log}")
file(COPY_FILE "${GUI}" "${app_root}/scenario_nmr_client.exe" ONLY_IF_DIFFERENT)
file(COPY_FILE "${FAKE_SDK}" "${runtime_root}/mridll.dll" ONLY_IF_DIFFERENT)
file(WRITE "${runtime_root}/hw_cfg/init.ini" "test")
file(WRITE "${runtime_root}/profiles/PTScan.par" "test")
file(SHA256 "${runtime_root}/mridll.dll" sdk_hash)
file(SHA256 "${runtime_root}/hw_cfg/init.ini" init_hash)
string(TOUPPER "${init_hash}" init_hash)
file(SIZE "${runtime_root}/hw_cfg/init.ini" init_size)
string(SHA256 hw_cfg_hash "init.ini|${init_size}|${init_hash}")
file(SHA256 "${runtime_root}/profiles/PTScan.par" par_hash)
set(test_expectations "${sdk_hash}|${init_hash}|${par_hash}|1|${init_size}|${hw_cfg_hash}")
file(WRITE "${runtime_root}/mri-runtime-manifest.json"
    "{\n"
    "  \"mridll\": { \"relativePath\": \"mridll.dll\", \"sha256\": \"${sdk_hash}\" },\n"
    "  \"hwCfg\": { \"relativePath\": \"hw_cfg\", \"fileCount\": 1, \"totalBytes\": ${init_size}, \"manifestSha256\": \"${hw_cfg_hash}\", \"initSha256\": \"${init_hash}\" },\n"
    "  \"parameterFile\": { \"fileName\": \"PTScan.par\", \"sha256\": \"${par_hash}\" }\n"
    "}\n")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "PATH=C:/msys64/ucrt64/bin;$ENV{PATH}"
        "QT_QPA_PLATFORM=offscreen"
        "FAKE_CALL_LOG=${call_log}"
        "MRI_RUNTIME_TEST_EXPECTATIONS=${test_expectations}"
        "${app_root}/scenario_nmr_client.exe"
        --auto-connect
    RESULT_VARIABLE gui_result
    OUTPUT_VARIABLE gui_output
    ERROR_VARIABLE gui_error
    TIMEOUT 2
)

if(NOT gui_result MATCHES "timeout")
    message(FATAL_ERROR
        "Qt GUI did not remain in its event loop: ${gui_result}\n"
        "stdout:\n${gui_output}\n"
        "stderr:\n${gui_error}")
endif()
if(NOT EXISTS "${call_log}")
    message(FATAL_ERROR "Qt GUI did not call the fake SDK\nstdout:\n${gui_output}\nstderr:\n${gui_error}")
endif()

file(STRINGS "${call_log}" calls)
set(init_count 0)
set(run_count 0)
set(abort_count 0)
set(connect_count 0)
foreach(call IN LISTS calls)
    if(call STREQUAL "Init")
        math(EXPR init_count "${init_count} + 1")
    elseif(call STREQUAL "Run")
        math(EXPR run_count "${run_count} + 1")
    elseif(call STREQUAL "Abort")
        math(EXPR abort_count "${abort_count} + 1")
    elseif(call STREQUAL "GetConnectStatus")
        math(EXPR connect_count "${connect_count} + 1")
    endif()
endforeach()

if(NOT init_count EQUAL 1)
    message(FATAL_ERROR "Expected exactly one SDK Init call, got ${init_count}: ${calls}")
endif()
if(NOT run_count EQUAL 0)
    message(FATAL_ERROR "GUI startup must not trigger Run: ${calls}")
endif()
if(NOT abort_count EQUAL 0)
    message(FATAL_ERROR "GUI startup must not trigger Abort: ${calls}")
endif()
if(connect_count LESS 1)
    message(FATAL_ERROR "Qt GUI did not finish the automatic device connection: ${calls}")
endif()

set(invalid_app_root "${test_root}/invalid-app")
set(invalid_call_log "${test_root}/invalid-calls.log")
set(invalid_trace "${test_root}/invalid-trace.log")
set(invalid_runtime_root "${invalid_app_root}/mri-runtime")
file(MAKE_DIRECTORY "${invalid_runtime_root}/hw_cfg" "${invalid_runtime_root}/profiles" "${invalid_app_root}/mri-output")
file(REMOVE "${invalid_trace}")
file(WRITE "${invalid_call_log}" "")
file(COPY_FILE "${GUI}" "${invalid_app_root}/scenario_nmr_client.exe" ONLY_IF_DIFFERENT)
file(COPY_FILE "${FAKE_SDK}" "${invalid_runtime_root}/mridll.dll" ONLY_IF_DIFFERENT)
file(WRITE "${invalid_runtime_root}/hw_cfg/init.ini" "test")
file(WRITE "${invalid_runtime_root}/profiles/PTScan.par" "test")
file(WRITE "${invalid_runtime_root}/mri-runtime-manifest.json"
    "{\n"
    "  \"mridll\": { \"relativePath\": \"mridll.dll\", \"sha256\": \"NOT_THE_FAKE_DLL_HASH\" },\n"
    "  \"hwCfg\": { \"relativePath\": \"hw_cfg\", \"fileCount\": 1, \"totalBytes\": ${init_size}, \"manifestSha256\": \"${hw_cfg_hash}\", \"initSha256\": \"${init_hash}\" },\n"
    "  \"parameterFile\": { \"fileName\": \"PTScan.par\", \"sha256\": \"${par_hash}\" }\n"
    "}\n")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "PATH=C:/msys64/ucrt64/bin;$ENV{PATH}"
        "QT_QPA_PLATFORM=offscreen"
        "FAKE_CALL_LOG=${invalid_call_log}"
        "MRI_RUNTIME_TEST_EXPECTATIONS=${test_expectations}"
        "MRI_RUNTIME_TEST_TRACE_FILE=${invalid_trace}"
        "${invalid_app_root}/scenario_nmr_client.exe"
        --auto-connect
    RESULT_VARIABLE invalid_result
    OUTPUT_VARIABLE invalid_output
    ERROR_VARIABLE invalid_error
    TIMEOUT 2
)
if(NOT invalid_result MATCHES "timeout")
    message(FATAL_ERROR
        "Invalid bundled runtime must leave the GUI event loop open: ${invalid_result}\n"
        "stdout:\n${invalid_output}\n"
        "stderr:\n${invalid_error}")
endif()
file(STRINGS "${invalid_call_log}" invalid_calls)
set(invalid_init_count 0)
set(invalid_run_count 0)
set(invalid_abort_count 0)
foreach(call IN LISTS invalid_calls)
    if(call STREQUAL "Init")
        math(EXPR invalid_init_count "${invalid_init_count} + 1")
    elseif(call STREQUAL "Run")
        math(EXPR invalid_run_count "${invalid_run_count} + 1")
    elseif(call STREQUAL "Abort")
        math(EXPR invalid_abort_count "${invalid_abort_count} + 1")
    endif()
endforeach()
if(NOT invalid_init_count EQUAL 0 OR NOT invalid_run_count EQUAL 0 OR NOT invalid_abort_count EQUAL 0)
    message(FATAL_ERROR "Invalid bundled runtime must not call Init, Run, or Abort: ${invalid_calls}")
endif()
if(NOT EXISTS "${invalid_trace}")
    message(FATAL_ERROR "Invalid bundled runtime did not record a resolution error")
endif()
file(READ "${invalid_trace}" invalid_trace_content)
if(NOT invalid_trace_content MATCHES "Automatic device connection skipped")
    message(FATAL_ERROR "Invalid bundled runtime error trace is unclear: ${invalid_trace_content}")
endif()
