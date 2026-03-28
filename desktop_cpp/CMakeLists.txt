cmake_minimum_required(VERSION 3.21)
project(AlnorisBMSx VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_add_executable(alnoris_desktop WIN32
    cad/CadEngine.cpp
    cad/CadEngine.h
    cad/battery/BatteryConfig.h
    cad/battery/BatteryEntities.h
    cad/battery/BatteryVisualizationBuilder.cpp
    cad/battery/BatteryVisualizationBuilder.h
    cad/battery/PackLayoutGenerator.cpp
    cad/battery/PackLayoutGenerator.h
    cad/camera/Camera.cpp
    cad/camera/Camera.h
    cad/commands/BatteryCommands.cpp
    cad/commands/BatteryCommands.h
    cad/commands/CommandStack.cpp
    cad/commands/CommandStack.h
    cad/commands/ICommand.h
    cad/edit/CadEditService.cpp
    cad/edit/CadEditService.h
    cad/core/CadDocument.cpp
    cad/core/CadDocument.h
    cad/core/EntityId.h
    cad/core/SelectionState.h
    cad/io/JsonCadDocumentIO.h
    cad/io/MeshLoader.cpp
    cad/io/MeshLoader.h
    cad/math/CadMath.h
    cad/picking/HitTester.cpp
    cad/picking/HitTester.h
    cad/render/IRenderBackend.h
    cad/render/RenderComposer.cpp
    cad/render/RenderComposer.h
    cad/render/RenderPacket.h
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
    src/SimulationMappingBuilder.cpp
    src/SimulationMappingBuilder.h
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

if (WIN32)
    get_target_property(QT_CORE_DLL Qt6::Core IMPORTED_LOCATION_DEBUG)
    if (NOT QT_CORE_DLL)
        get_target_property(QT_CORE_DLL Qt6::Core IMPORTED_LOCATION_RELEASE)
    endif()

    if (QT_CORE_DLL)
        get_filename_component(QT_BIN_DIR "${QT_CORE_DLL}" DIRECTORY)
        get_filename_component(QT_PREFIX_DIR "${QT_BIN_DIR}" DIRECTORY)
        set(QT_PLATFORMS_DIR "${QT_PREFIX_DIR}/plugins/platforms")

        add_custom_command(TARGET alnoris_desktop POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:alnoris_desktop>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_PLATFORMS_DIR}/qwindowsd.dll"
                "$<TARGET_FILE_DIR:alnoris_desktop>/platforms/qwindowsd.dll"
        )

        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/qt.conf" "[Paths]\nPlugins = .\n")
        add_custom_command(TARGET alnoris_desktop POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_BINARY_DIR}/qt.conf"
                "$<TARGET_FILE_DIR:alnoris_desktop>/qt.conf"
        )
    endif()
endif()

if (MSVC)
    target_compile_options(alnoris_desktop PRIVATE /permissive- /Zc:__cplusplus)
endif()

target_compile_definitions(alnoris_desktop PRIVATE
    ALNORIS_PROJECT_ROOT="${CMAKE_SOURCE_DIR}/.."
    ALNORIS_DEFAULT_CELL_STL="C:/Users/Craig/OneDrive/Desktop/Untitled.stl"
)
