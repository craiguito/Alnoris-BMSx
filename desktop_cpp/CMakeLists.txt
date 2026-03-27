cmake_minimum_required(VERSION 3.21)
project(AlnorisBMSx VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_add_executable(alnoris_desktop WIN32
    cad/CadModule.cpp
    cad/CadModule.h
    cad/CadTypes.h
    charts/ChartModule.cpp
    charts/ChartModule.h
    charts/ChartTypes.h
    src/main.cpp
    src/CadViewportWidget.cpp
    src/CadViewportWidget.h
    src/ChartWidget.cpp
    src/ChartWidget.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/SimulationClient.cpp
    src/SimulationClient.h
)

target_link_libraries(alnoris_desktop
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

add_custom_command(TARGET alnoris_desktop POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:alnoris_desktop>
        $<TARGET_FILE_DIR:alnoris_desktop>
    COMMAND_EXPAND_LISTS
)

if (MSVC)
    target_compile_options(alnoris_desktop PRIVATE /permissive- /Zc:__cplusplus)
endif()

target_compile_definitions(alnoris_desktop PRIVATE
    ALNORIS_PROJECT_ROOT="${CMAKE_SOURCE_DIR}/.."
    ALNORIS_DEFAULT_CELL_STL="C:/Users/Craig/OneDrive/Desktop/Untitled.stl"
)
