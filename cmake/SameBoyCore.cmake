# Builds the SameBoy emulation core straight from the unmodified upstream
# submodule. Nothing here lists individual core files, so upstream additions
# are picked up automatically after a submodule bump.

set(SAMEBOY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SameBoy" CACHE PATH "Path to upstream SameBoy sources")

if(NOT EXISTS "${SAMEBOY_DIR}/Core/gb.h")
    message(FATAL_ERROR "SameBoy sources not found in ${SAMEBOY_DIR}. Run: git submodule update --init")
endif()

# Version and copyright year, exactly as upstream's Makefile derives them.
file(STRINGS "${SAMEBOY_DIR}/version.mk" _sameboy_version_line REGEX "^VERSION[ \t]*:=")
string(REGEX REPLACE "^VERSION[ \t]*:=[ \t]*" "" SAMEBOY_VERSION "${_sameboy_version_line}")
string(STRIP "${SAMEBOY_VERSION}" SAMEBOY_VERSION)
file(STRINGS "${SAMEBOY_DIR}/LICENSE" _sameboy_license_year REGEX "20[2-9][0-9]")
string(REGEX MATCH "20[2-9][0-9]" SAMEBOY_COPYRIGHT_YEAR "${_sameboy_license_year}")
message(STATUS "SameBoy core version: ${SAMEBOY_VERSION} (© ${SAMEBOY_COPYRIGHT_YEAR})")

file(GLOB SAMEBOY_CORE_SOURCES CONFIGURE_DEPENDS "${SAMEBOY_DIR}/Core/*.c")
if(WIN32)
    file(GLOB _sameboy_windows_sources CONFIGURE_DEPENDS "${SAMEBOY_DIR}/Windows/*.c")
    list(APPEND SAMEBOY_CORE_SOURCES ${_sameboy_windows_sources})
endif()

add_library(sameboy_core STATIC ${SAMEBOY_CORE_SOURCES})
set_target_properties(sameboy_core PROPERTIES
    C_STANDARD 11
    C_EXTENSIONS ON
    POSITION_INDEPENDENT_CODE ON)

# Consumers include <Core/gb.h>, like upstream frontends do.
target_include_directories(sameboy_core PUBLIC "${SAMEBOY_DIR}")
if(WIN32)
    target_include_directories(sameboy_core PRIVATE "${SAMEBOY_DIR}/Windows")
endif()

target_compile_definitions(sameboy_core
    PRIVATE GB_INTERNAL _GNU_SOURCE _USE_MATH_DEFINES
            "GB_VERSION=\"${SAMEBOY_VERSION}\""
            "GB_COPYRIGHT_YEAR=\"${SAMEBOY_COPYRIGHT_YEAR}\""
    PUBLIC  "GB_VERSION=\"${SAMEBOY_VERSION}\""
            "GB_COPYRIGHT_YEAR=\"${SAMEBOY_COPYRIGHT_YEAR}\"")

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    # Mirror upstream's warning suppressions; the core is upstream's code, so
    # we don't want local warnings policy to break the build on a sync.
    target_compile_options(sameboy_core PRIVATE
        -Wno-missing-braces -Wno-nonnull -Wno-unused-result -Wno-multichar
        -Wno-int-in-bool-context -Wno-format-truncation
        $<$<C_COMPILER_ID:Clang>:-Wno-nullability-completeness -Wno-unknown-warning-option>
        $<$<C_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>
        $<$<NOT:$<CONFIG:Debug>>:-O3 -ffast-math>)
endif()

find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    target_link_libraries(sameboy_core PUBLIC ${MATH_LIBRARY})
endif()
