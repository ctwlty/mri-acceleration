cmake_minimum_required(VERSION 3.21)

project(scenario_nmr_client LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(scenario_nmr_client
    src/main.cpp
    src/app/DeviceActionAvailability.cpp
    src/app/DeviceActionAvailability.h
    src/app/MriSdkLoader.cpp
    src/app/MriSdkLoader.h
    src/app/MriRuntimeResolver.cpp
    src/app/MriRuntimeResolver.h
    src/app/ProtocolMapper.cpp
    src/app/ProtocolMapper.h
    src/app/DeviceBridge.cpp
    src/app/DeviceBridge.h
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/app/SceneCatalog.cpp
    src/app/SceneCatalog.h
    src/app/SceneTemplate.h
    resources/app.qrc
)

target_include_directories(scenario_nmr_client PRIVATE src)

target_link_libraries(scenario_nmr_client PRIVATE Qt6::Widgets)

if(BUILD_TESTING)
    qt_add_executable(scenario_nmr_client_test
        src/main.cpp
        src/app/DeviceActionAvailability.cpp
        src/app/DeviceActionAvailability.h
        src/app/MriSdkLoader.cpp
        src/app/MriSdkLoader.h
        src/app/MriRuntimeResolver.cpp
        src/app/MriRuntimeResolver.h
        src/app/ProtocolMapper.cpp
        src/app/ProtocolMapper.h
        src/app/DeviceBridge.cpp
        src/app/DeviceBridge.h
        src/app/MainWindow.cpp
        src/app/MainWindow.h
        src/app/SceneCatalog.cpp
        src/app/SceneCatalog.h
        src/app/SceneTemplate.h
        resources/app.qrc
    )
    target_include_directories(scenario_nmr_client_test PRIVATE src)
    target_link_libraries(scenario_nmr_client_test PRIVATE Qt6::Widgets)
    target_compile_definitions(scenario_nmr_client_test PRIVATE MRI_RUNTIME_RESOLVER_TESTING)
endif()

qt_add_executable(mri_sdk_verify
    tools/mri_sdk_verify.cpp
    src/app/DeviceBridge.cpp
    src/app/DeviceBridge.h
    src/app/MriSdkLoader.cpp
    src/app/MriSdkLoader.h
    src/app/MriSdkTypes.h
    src/app/ProtocolMapper.cpp
    src/app/ProtocolMapper.h
    src/app/SceneTemplate.h
)
target_include_directories(mri_sdk_verify PRIVATE src)
target_link_libraries(mri_sdk_verify PRIVATE Qt6::Core)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    find_program(POWERSHELL_EXECUTABLE NAMES powershell.exe powershell REQUIRED)

    add_library(fake_mri_sdk SHARED
        tests/fake_mri_sdk.cpp
    )
    set_target_properties(fake_mri_sdk PROPERTIES
        PREFIX ""
        OUTPUT_NAME "fake_mri_sdk"
    )

    add_library(fake_incomplete_mri_sdk SHARED
        tests/fake_incomplete_mri_sdk.cpp
    )
    set_target_properties(fake_incomplete_mri_sdk PROPERTIES
        PREFIX ""
        OUTPUT_NAME "fake_incomplete_mri_sdk"
    )

    qt_add_executable(test_mri_sdk_loader
        tests/test_mri_sdk_loader.cpp
        src/app/MriSdkLoader.cpp
        src/app/MriSdkLoader.h
        src/app/MriSdkTypes.h
        src/app/SceneTemplate.h
    )
    target_include_directories(test_mri_sdk_loader PRIVATE src)
    target_link_libraries(test_mri_sdk_loader PRIVATE Qt6::Core Qt6::Test)

    qt_add_executable(test_device_bridge
        tests/test_device_bridge.cpp
        src/app/DeviceBridge.cpp
        src/app/DeviceBridge.h
        src/app/MriSdkLoader.cpp
        src/app/MriSdkLoader.h
        src/app/MriSdkTypes.h
        src/app/ProtocolMapper.cpp
        src/app/ProtocolMapper.h
        src/app/SceneTemplate.h
    )
    target_include_directories(test_device_bridge PRIVATE src)
    target_link_libraries(test_device_bridge PRIVATE Qt6::Core Qt6::Test)

    qt_add_executable(test_device_actions
        tests/test_device_actions.cpp
        src/app/DeviceActionAvailability.cpp
        src/app/DeviceActionAvailability.h
    )
    target_include_directories(test_device_actions PRIVATE src)
    target_link_libraries(test_device_actions PRIVATE Qt6::Core Qt6::Test)

    qt_add_executable(test_mri_runtime_resolver
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mri_runtime_resolver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/MriRuntimeResolver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/MriRuntimeResolver.h
    )
    target_include_directories(test_mri_runtime_resolver PRIVATE src)
    target_link_libraries(test_mri_runtime_resolver PRIVATE Qt6::Core Qt6::Test)
    target_compile_definitions(test_mri_runtime_resolver PRIVATE MRI_RUNTIME_RESOLVER_TESTING)

    qt_add_executable(test_main_window
        tests/test_main_window.cpp
        src/app/DeviceActionAvailability.cpp
        src/app/DeviceActionAvailability.h
        src/app/DeviceBridge.cpp
        src/app/DeviceBridge.h
        src/app/MainWindow.cpp
        src/app/MainWindow.h
        src/app/MriSdkLoader.cpp
        src/app/MriSdkLoader.h
        src/app/ProtocolMapper.cpp
        src/app/ProtocolMapper.h
        src/app/SceneCatalog.cpp
        src/app/SceneCatalog.h
        src/app/SceneTemplate.h
    )
    target_include_directories(test_main_window PRIVATE src)
    target_link_libraries(test_main_window PRIVATE Qt6::Widgets Qt6::Test)

    add_test(NAME mri_sdk_loader COMMAND test_mri_sdk_loader)
    set_tests_properties(mri_sdk_loader PROPERTIES
        ENVIRONMENT "FAKE_MRI_SDK_PATH=$<TARGET_FILE:fake_mri_sdk>;FAKE_INCOMPLETE_MRI_SDK_PATH=$<TARGET_FILE:fake_incomplete_mri_sdk>"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:C:\\msys64\\ucrt64\\bin"
    )

    add_test(NAME device_bridge COMMAND test_device_bridge)
    set_tests_properties(device_bridge PROPERTIES
        ENVIRONMENT "FAKE_MRI_SDK_PATH=$<TARGET_FILE:fake_mri_sdk>"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:C:\\msys64\\ucrt64\\bin"
    )

    add_test(NAME device_actions COMMAND test_device_actions)
    set_tests_properties(device_actions PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:C:\\msys64\\ucrt64\\bin"
    )

    add_test(NAME mri_runtime_resolver COMMAND test_mri_runtime_resolver)
    set_tests_properties(mri_runtime_resolver PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:C:\\msys64\\ucrt64\\bin"
    )

    add_test(NAME main_window COMMAND test_main_window)
    set_tests_properties(main_window PROPERTIES
        ENVIRONMENT "FAKE_MRI_SDK_PATH=$<TARGET_FILE:fake_mri_sdk>;QT_QPA_PLATFORM=offscreen"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:C:\\msys64\\ucrt64\\bin"
    )

    add_test(
        NAME sdk_verify_fake
        COMMAND ${CMAKE_COMMAND}
            "-DVERIFIER=$<TARGET_FILE:mri_sdk_verify>"
            "-DFAKE_SDK=$<TARGET_FILE:fake_mri_sdk>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_sdk_verify_test.cmake"
    )
    set_tests_properties(sdk_verify_fake PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:C:\\msys64\\ucrt64\\bin"
    )

    add_test(
        NAME gui_auto_connect_fake
        COMMAND ${CMAKE_COMMAND}
            "-DGUI=$<TARGET_FILE:scenario_nmr_client_test>"
            "-DFAKE_SDK=$<TARGET_FILE:fake_mri_sdk>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_gui_auto_connect_test.cmake"
    )
    set_tests_properties(gui_auto_connect_fake PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:C:\\msys64\\ucrt64\\bin"
    )

    add_test(
        NAME stage_mri_runtime
        COMMAND ${POWERSHELL_EXECUTABLE} -NoProfile -ExecutionPolicy Bypass -File
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_stage_mri_runtime.ps1"
    )
endif()
