if(NOT DEFINED GUI OR NOT EXISTS "${GUI}")
    message(FATAL_ERROR "Qt GUI executable is missing: ${GUI}")
endif()
if(NOT DEFINED FAKE_PROXY OR NOT EXISTS "${FAKE_PROXY}")
    message(FATAL_ERROR "Fake eggcontroller proxy is missing: ${FAKE_PROXY}")
endif()

set(test_root "${CMAKE_CURRENT_BINARY_DIR}/gui-automation-mode-test")
set(egg_root "${test_root}/eggcontrollerV2")
set(proxy_script "${test_root}/eggcontroller_proxy.py")
file(MAKE_DIRECTORY "${egg_root}")
file(WRITE "${proxy_script}" "# test proxy\n")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "PATH=C:/msys64/ucrt64/bin;$ENV{PATH}"
        "QT_QPA_PLATFORM=offscreen"
        "${GUI}"
        --automation-python "${FAKE_PROXY}"
        --automation-root "${egg_root}"
        --automation-proxy "${proxy_script}"
    RESULT_VARIABLE gui_result
    OUTPUT_VARIABLE gui_output
    ERROR_VARIABLE gui_error
    TIMEOUT 3
)

if(NOT "${gui_result}" MATCHES "timeout")
    message(FATAL_ERROR
        "Automation-configured GUI did not remain idle in its event loop: ${gui_result}\n"
        "stdout:\n${gui_output}\n"
        "stderr:\n${gui_error}")
endif()
