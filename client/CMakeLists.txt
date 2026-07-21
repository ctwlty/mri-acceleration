cmake_minimum_required(VERSION 3.21)

project(scenario_nmr_client LANGUAGES CXX)

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
