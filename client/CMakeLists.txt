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
    src/app/MriSdkLoader.cpp
    src/app/MriSdkLoader.h
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
    find_package(Qt6 REQUIRED COMPONENTS Test)

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
endif()
