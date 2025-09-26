cmake_minimum_required(VERSION 3.31)
project(qtTest)

find_package(OpenCV REQUIRED)

file(COPY ${CMAKE_SOURCE_DIR}/src/resource/config.json
        DESTINATION ${CMAKE_BINARY_DIR}/src/resource)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "D:/QT/6.9.2/mingw_64")

find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        REQUIRED)

add_executable(qtTest main.cpp
        src/widget/main/mainwindow.cpp
        src/widget/main/mainwindow.h
        src/widget/main/mainwindow.ui
        ExecutionSteps.cpp
        ExecutionSteps.h
        src/common/ConfigTypeEnum.cpp
        src/common/ConfigTypeEnum.h
        src/common/Logger.cpp
        src/common/Logger.h
)

add_definitions(-DPROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

target_link_libraries(qtTest
        PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        ${OpenCV_LIBS}
)

include_directories(
        ${CMAKE_SOURCE_DIR}/src/common
)
include_directories(
        ${CMAKE_SOURCE_DIR}
)

if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(DEBUG_SUFFIX)
    if (MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif ()
    set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")
    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        endif ()
    endif ()
    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    endif ()
    foreach (QT_LIB Core Gui Widgets)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endforeach (QT_LIB)
endif ()
