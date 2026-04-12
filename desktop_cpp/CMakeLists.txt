cmake_minimum_required(VERSION 3.21)
project(AlnorisBMSx VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(ALNORIS_PREFERRED_QT_ROOT "")
file(GLOB ALNORIS_QT_CANDIDATES LIST_DIRECTORIES true "C:/Qt/*/msvc*_64")
list(SORT ALNORIS_QT_CANDIDATES COMPARE NATURAL ORDER DESCENDING)

foreach(ALNORIS_QT_ROOT IN LISTS ALNORIS_QT_CANDIDATES)
    if (EXISTS "${ALNORIS_QT_ROOT}/lib/cmake/Qt6/Qt6Config.cmake")
        set(ALNORIS_PREFERRED_QT_ROOT "${ALNORIS_QT_ROOT}")
        break()
    endif()
endforeach()

if (ALNORIS_PREFERRED_QT_ROOT)
    set(ALNORIS_PREFERRED_QT_CMAKE_DIR "${ALNORIS_PREFERRED_QT_ROOT}/lib/cmake")
    if (NOT Qt6_DIR OR Qt6_DIR MATCHES "FreeCAD" OR NOT EXISTS "${Qt6_DIR}/Qt6Config.cmake")
        set(Qt6_DIR "${ALNORIS_PREFERRED_QT_CMAKE_DIR}/Qt6" CACHE PATH "Preferred Qt6 package directory" FORCE)
        foreach(ALNORIS_QT_COMPONENT
            Qt6Core
            Qt6CoreTools
            Qt6EntryPointPrivate
            Qt6Gui
            Qt6GuiTools
            Qt6OpenGL
            Qt6OpenGLWidgets
            Qt6Widgets
            Qt6WidgetsTools
        )
            if (EXISTS "${ALNORIS_PREFERRED_QT_CMAKE_DIR}/${ALNORIS_QT_COMPONENT}/${ALNORIS_QT_COMPONENT}Config.cmake")
                set(${ALNORIS_QT_COMPONENT}_DIR
                    "${ALNORIS_PREFERRED_QT_CMAKE_DIR}/${ALNORIS_QT_COMPONENT}"
                    CACHE PATH "Preferred ${ALNORIS_QT_COMPONENT} package directory" FORCE
                )
            endif()
        endforeach()
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)

qt_add_executable(alnoris_desktop WIN32
    cad/CadEngine.cpp
    cad/CadEngine.h
    cad/battery/BatteryConfig.h
    cad/battery/BatteryEntities.h
    cad/battery/BatteryVisualizationBuilder.cpp
    cad/battery/BatteryVisualizationBuilder.h
    cad/battery/PackLayoutGenerator.cpp
    cad/battery/PackLayoutGenerator.h
    cad/battery/PackLayoutMetrics.cpp
    cad/battery/PackLayoutMetrics.h
    cad/camera/Camera.cpp
    cad/camera/Camera.h
    cad/commands/BatteryCommands.cpp
    cad/commands/BatteryCommands.h
    cad/commands/CommandStack.cpp
    cad/commands/CommandStack.h
    cad/commands/ICommand.h
    cad/edit/CadEditService.cpp
    cad/edit/CadEditService.h
    cad/geometry/BatteryGeometryGenerator.cpp
    cad/geometry/BatteryGeometryGenerator.h
    cad/geometry/ParametricPrimitives.cpp
    cad/geometry/ParametricPrimitives.h
    cad/core/CadDocument.cpp
    cad/core/CadDocument.h
    cad/core/EntityId.h
    cad/core/SelectionState.h
    cad/io/JsonCadDocumentIO.h
    cad/io/JsonCadDocumentIO.cpp
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
    src/SimulationResultModel.cpp
    src/SimulationResultModel.h
    src/SimulationMappingBuilder.cpp
    src/SimulationMappingBuilder.h
)

target_link_libraries(alnoris_desktop
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::OpenGL
        Qt6::OpenGLWidgets
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
)

add_executable(cad_regression_tests
    cad/CadEngine.cpp
    cad/CadEngine.h
    cad/battery/BatteryVisualizationBuilder.cpp
    cad/battery/BatteryVisualizationBuilder.h
    cad/battery/PackLayoutGenerator.cpp
    cad/battery/PackLayoutGenerator.h
    cad/battery/PackLayoutMetrics.cpp
    cad/battery/PackLayoutMetrics.h
    cad/camera/Camera.cpp
    cad/camera/Camera.h
    cad/commands/BatteryCommands.cpp
    cad/commands/BatteryCommands.h
    cad/commands/CommandStack.cpp
    cad/commands/CommandStack.h
    cad/commands/ICommand.h
    cad/core/CadDocument.cpp
    cad/core/CadDocument.h
    cad/core/EntityId.h
    cad/core/SelectionState.h
    cad/edit/CadEditService.cpp
    cad/edit/CadEditService.h
    cad/geometry/BatteryGeometryGenerator.cpp
    cad/geometry/BatteryGeometryGenerator.h
    cad/geometry/ParametricPrimitives.cpp
    cad/geometry/ParametricPrimitives.h
    cad/io/MeshLoader.cpp
    cad/io/MeshLoader.h
    cad/io/JsonCadDocumentIO.cpp
    cad/io/JsonCadDocumentIO.h
    cad/math/CadMath.h
    cad/picking/HitTester.cpp
    cad/picking/HitTester.h
    cad/render/RenderComposer.cpp
    cad/render/RenderComposer.h
    cad/render/RenderPacket.h
    src/SimulationMappingBuilder.cpp
    src/SimulationMappingBuilder.h
    tests/CadRegressionTests.cpp
    tests/TestHarness.h
    tests/TestMain.cpp
)

target_include_directories(cad_regression_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cad_regression_tests
    PRIVATE
        Qt6::Core
)

set(ALNORIS_CAD_CTEST_FILE_CONTENT
    "add_test([=[cad_regression_suite]=] \"${CMAKE_COMMAND}\" \"-DTEST_BINARY_DIR=${CMAKE_BINARY_DIR}\" \"-P\" \"${CMAKE_CURRENT_SOURCE_DIR}/tests/RunCadRegressionTests.cmake\")\n"
)

file(GENERATE
    OUTPUT "${CMAKE_BINARY_DIR}/cad_regression_ctest_file.cmake"
    CONTENT "${ALNORIS_CAD_CTEST_FILE_CONTENT}"
)

set_target_properties(cad_regression_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests-bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/tests-bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/tests-bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/tests-bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/tests-bin"
)

add_custom_command(TARGET cad_regression_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:cad_regression_tests>
        $<TARGET_FILE_DIR:cad_regression_tests>
    COMMAND_EXPAND_LISTS
)

add_custom_command(TARGET cad_regression_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/cad_regression_ctest_file.cmake"
        "${CMAKE_BINARY_DIR}/CTestTestfile.cmake"
)

if (MSVC)
    target_compile_options(cad_regression_tests PRIVATE /permissive- /Zc:__cplusplus)
endif()
