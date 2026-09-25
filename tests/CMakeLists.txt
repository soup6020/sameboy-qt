find_package(Qt6 REQUIRED COMPONENTS Test)
find_program(RGBASM_EXECUTABLE rgbasm)
find_program(RGBLINK_EXECUTABLE rgblink)
find_program(RGBFIX_EXECUTABLE rgbfix)
if(NOT RGBASM_EXECUTABLE OR NOT RGBLINK_EXECUTABLE OR NOT RGBFIX_EXECUTABLE)
    message(STATUS "RGBDS not found: automated tests disabled")
    return()
endif()

set(TEST_ROM "${CMAKE_CURRENT_BINARY_DIR}/testrom.gb")
add_custom_command(OUTPUT "${TEST_ROM}"
    COMMAND ${RGBASM_EXECUTABLE} -o testrom.o "${CMAKE_CURRENT_SOURCE_DIR}/testrom.asm"
    COMMAND ${RGBLINK_EXECUTABLE} -o testrom.gb testrom.o
    COMMAND ${RGBFIX_EXECUTABLE} -v -p 0xFF -m 0x1B -r 0x02 -t SBQTTEST testrom.gb
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/testrom.asm"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    VERBATIM)
add_custom_target(sameboy_test_rom DEPENDS "${TEST_ROM}")

qt_add_executable(sameboy-qt-tests tst_sameboy.cpp)
add_dependencies(sameboy-qt-tests sameboy_test_rom sameboy_assets)
target_link_libraries(sameboy-qt-tests PRIVATE sameboy-qt-lib Qt6::Test)
target_compile_definitions(sameboy-qt-tests PRIVATE
    "TEST_ROM=\"${TEST_ROM}\""
    "TEST_DATA_DIR=\"${CMAKE_BINARY_DIR}/share/sameboy-qt\"")
add_test(NAME sameboy-qt-tests COMMAND sameboy-qt-tests)
set_tests_properties(sameboy-qt-tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;SDL_AUDIO_DRIVER=dummy")
