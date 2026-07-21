if(NOT DEFINED GUI OR NOT EXISTS "${GUI}")
    message(FATAL_ERROR "Qt GUI executable is missing: ${GUI}")
endif()
if(NOT DEFINED FAKE_SDK OR NOT EXISTS "${FAKE_SDK}")
    message(FATAL_ERROR "Fake SDK is missing: ${FAKE_SDK}")
endif()

set(test_root "${CMAKE_CURRENT_BINARY_DIR}/gui-auto-connect-test")
set(sdk_root "${test_root}/sdk")
set(output_root "${test_root}/output")
set(call_log "${test_root}/calls.log")
file(MAKE_DIRECTORY "${sdk_root}/hw_cfg" "${output_root}")
file(REMOVE "${call_log}")
file(COPY_FILE "${FAKE_SDK}" "${sdk_root}/fake_mri_sdk.dll" ONLY_IF_DIFFERENT)
file(WRITE "${sdk_root}/hw_cfg/init.ini" "test")
file(WRITE "${test_root}/PTScan.par" "test")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "PATH=C:/msys64/ucrt64/bin;$ENV{PATH}"
        "QT_QPA_PLATFORM=offscreen"
        "FAKE_CALL_LOG=${call_log}"
        "${GUI}"
        --auto-connect
        --sdk "${sdk_root}/fake_mri_sdk.dll"
        --par "${test_root}/PTScan.par"
        --output "${output_root}"
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
    message(FATAL_ERROR "Qt GUI did not call the fake SDK")
endif()

file(STRINGS "${call_log}" calls)
set(init_count 0)
set(run_count 0)
set(connect_count 0)
foreach(call IN LISTS calls)
    if(call STREQUAL "Init")
        math(EXPR init_count "${init_count} + 1")
    elseif(call STREQUAL "Run")
        math(EXPR run_count "${run_count} + 1")
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
if(connect_count LESS 1)
    message(FATAL_ERROR "Qt GUI did not finish the automatic device connection: ${calls}")
endif()
