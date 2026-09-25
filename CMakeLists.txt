cmake_minimum_required(VERSION 3.21)

project(sameboy-qt LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(SAMEBOY_QT_CAMERA "Game Boy Camera support via Qt Multimedia" ON)

include(cmake/SameBoyCore.cmake)
include(cmake/SameBoyAssets.cmake)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets PrintSupport)
if(SAMEBOY_QT_CAMERA)
    find_package(Qt6 6.4 COMPONENTS Multimedia)
    if(NOT Qt6Multimedia_FOUND)
        message(WARNING "Qt6 Multimedia not found; Game Boy Camera will produce black images")
        set(SAMEBOY_QT_CAMERA OFF)
    endif()
endif()
find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)

# Everything but main.cpp lives in a library shared by the app and the tests.
file(GLOB_RECURSE SAMEBOY_QT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
list(REMOVE_ITEM SAMEBOY_QT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(sameboy-qt-lib STATIC ${SAMEBOY_QT_SOURCES})
target_include_directories(sameboy-qt-lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(sameboy-qt-lib PUBLIC
    sameboy_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::PrintSupport
    SDL3::SDL3)
if(SAMEBOY_QT_CAMERA)
    target_link_libraries(sameboy-qt-lib PUBLIC Qt6::Multimedia)
    target_compile_definitions(sameboy-qt-lib PRIVATE SAMEBOY_QT_CAMERA=1)
endif()

include(GNUInstallDirs)
target_compile_definitions(sameboy-qt-lib PRIVATE
    "SAMEBOY_QT_INSTALL_DATADIR=\"${CMAKE_INSTALL_FULL_DATADIR}/sameboy-qt\"")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(sameboy-qt-lib PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

qt_add_executable(sameboy-qt WIN32 MACOSX_BUNDLE src/main.cpp resources/resources.qrc)
add_dependencies(sameboy-qt sameboy_assets)
target_link_libraries(sameboy-qt PRIVATE sameboy-qt-lib)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(sameboy-qt PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

option(SAMEBOY_QT_BUILD_TESTS "Build the automated tests (needs RGBDS for the test ROM)" ON)
if(SAMEBOY_QT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Run from the build tree: the executable sits next to share/sameboy-qt.
set_target_properties(sameboy-qt PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

install(TARGETS sameboy-qt
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .)
if(UNIX AND NOT APPLE)
    # Desktop integration, using upstream's FreeDesktop assets. Files are named
    # sameboy-qt* so they can coexist with an installed SDL SameBoy.
    install(FILES resources/sameboy-qt.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    foreach(size 16x16 32x32 64x64 128x128 256x256 512x512)
        install(FILES "${SAMEBOY_DIR}/FreeDesktop/AppIcon/${size}.png"
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}/apps RENAME sameboy-qt.png)
    endforeach()
    install(FILES "${SAMEBOY_DIR}/FreeDesktop/sameboy.xml"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages RENAME sameboy-qt.xml)
endif()
