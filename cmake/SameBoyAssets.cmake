# Stages runtime assets from the upstream submodule into the build tree:
#   <build>/share/sameboy-qt/BootROMs/*.bin
#   <build>/share/sameboy-qt/Shaders/*.fsh
#   <build>/share/sameboy-qt/registers.sym
# Boot ROMs are assembled by upstream's own Makefile (so toolchain quirks stay
# upstream's problem) unless SAMEBOY_BOOTROMS_DIR points at prebuilt files.

set(SAMEBOY_BOOTROMS_DIR "" CACHE PATH "Directory of prebuilt SameBoy boot ROMs (skips building them with RGBDS)")

set(SAMEBOY_ASSET_DIR "${CMAKE_BINARY_DIR}/share/sameboy-qt")
set(SAMEBOY_BOOT_ROMS dmg_boot mgb_boot cgb0_boot cgb_boot agb_boot sgb_boot sgb2_boot)

set(_bootrom_outputs)
foreach(rom IN LISTS SAMEBOY_BOOT_ROMS)
    list(APPEND _bootrom_outputs "${SAMEBOY_ASSET_DIR}/BootROMs/${rom}.bin")
endforeach()

if(SAMEBOY_BOOTROMS_DIR)
    set(_copy_commands)
    foreach(rom IN LISTS SAMEBOY_BOOT_ROMS)
        if(NOT EXISTS "${SAMEBOY_BOOTROMS_DIR}/${rom}.bin")
            message(FATAL_ERROR "SAMEBOY_BOOTROMS_DIR is missing ${rom}.bin")
        endif()
        list(APPEND _copy_commands COMMAND ${CMAKE_COMMAND} -E copy_if_different
             "${SAMEBOY_BOOTROMS_DIR}/${rom}.bin" "${SAMEBOY_ASSET_DIR}/BootROMs/${rom}.bin")
    endforeach()
    add_custom_command(OUTPUT ${_bootrom_outputs}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SAMEBOY_ASSET_DIR}/BootROMs"
        ${_copy_commands}
        COMMENT "Copying prebuilt SameBoy boot ROMs"
        VERBATIM)
else()
    find_program(RGBASM_EXECUTABLE rgbasm)
    find_program(MAKE_EXECUTABLE NAMES gmake make mingw32-make)
    if(NOT RGBASM_EXECUTABLE OR NOT MAKE_EXECUTABLE)
        message(FATAL_ERROR
            "Building SameBoy's boot ROMs needs RGBDS (rgbasm/rgblink/rgbgfx) and make.\n"
            "Either install RGBDS, or pass -DSAMEBOY_BOOTROMS_DIR=<dir with dmg_boot.bin etc>.")
    endif()
    get_filename_component(_rgbds_dir "${RGBASM_EXECUTABLE}" DIRECTORY)
    file(GLOB _bootrom_sources CONFIGURE_DEPENDS
        "${SAMEBOY_DIR}/BootROMs/*.asm" "${SAMEBOY_DIR}/BootROMs/*.inc"
        "${SAMEBOY_DIR}/BootROMs/*.png" "${SAMEBOY_DIR}/BootROMs/pb12.c")
    set(_upstream_bin "${CMAKE_BINARY_DIR}/sameboy-upstream")
    set(_copy_commands)
    foreach(rom IN LISTS SAMEBOY_BOOT_ROMS)
        list(APPEND _copy_commands COMMAND ${CMAKE_COMMAND} -E copy_if_different
             "${_upstream_bin}/BootROMs/${rom}.bin" "${SAMEBOY_ASSET_DIR}/BootROMs/${rom}.bin")
    endforeach()
    # All outputs are redirected into our build tree so the submodule stays clean.
    add_custom_command(OUTPUT ${_bootrom_outputs}
        COMMAND ${MAKE_EXECUTABLE} -C "${SAMEBOY_DIR}" bootroms
                "BIN=${_upstream_bin}" "OBJ=${_upstream_bin}/obj"
                "PB12_COMPRESS=${_upstream_bin}/pb12${CMAKE_EXECUTABLE_SUFFIX}"
                "RGBDS=${_rgbds_dir}/"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SAMEBOY_ASSET_DIR}/BootROMs"
        ${_copy_commands}
        DEPENDS ${_bootrom_sources}
        COMMENT "Assembling SameBoy boot ROMs with RGBDS"
        VERBATIM)
endif()

file(GLOB SAMEBOY_SHADERS CONFIGURE_DEPENDS "${SAMEBOY_DIR}/Shaders/*.fsh")
set(_asset_outputs)
foreach(shader IN LISTS SAMEBOY_SHADERS)
    get_filename_component(_name "${shader}" NAME)
    set(_out "${SAMEBOY_ASSET_DIR}/Shaders/${_name}")
    add_custom_command(OUTPUT "${_out}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${shader}" "${_out}"
        DEPENDS "${shader}" VERBATIM)
    list(APPEND _asset_outputs "${_out}")
endforeach()

add_custom_command(OUTPUT "${SAMEBOY_ASSET_DIR}/registers.sym"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SAMEBOY_DIR}/Misc/registers.sym" "${SAMEBOY_ASSET_DIR}/registers.sym"
    DEPENDS "${SAMEBOY_DIR}/Misc/registers.sym" VERBATIM)
list(APPEND _asset_outputs "${SAMEBOY_ASSET_DIR}/registers.sym")

# The SDL frontend's idle-screen logo, reused by the welcome window.
add_custom_command(OUTPUT "${SAMEBOY_ASSET_DIR}/background.bmp"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SAMEBOY_DIR}/SDL/background.bmp" "${SAMEBOY_ASSET_DIR}/background.bmp"
    DEPENDS "${SAMEBOY_DIR}/SDL/background.bmp" VERBATIM)
list(APPEND _asset_outputs "${SAMEBOY_ASSET_DIR}/background.bmp")

add_custom_command(OUTPUT "${SAMEBOY_ASSET_DIR}/LICENSE"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SAMEBOY_DIR}/LICENSE" "${SAMEBOY_ASSET_DIR}/LICENSE"
    DEPENDS "${SAMEBOY_DIR}/LICENSE" VERBATIM)
list(APPEND _asset_outputs "${SAMEBOY_ASSET_DIR}/LICENSE")

add_custom_target(sameboy_assets ALL DEPENDS ${_bootrom_outputs} ${_asset_outputs})

include(GNUInstallDirs)
install(DIRECTORY "${SAMEBOY_ASSET_DIR}/" DESTINATION "${CMAKE_INSTALL_DATADIR}/sameboy-qt")
