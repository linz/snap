# Builds the lapg parser-generator tool.
# Only required when regenerating src/snap_manager/scriptimp_parse.cpp from
# its .syntax grammar file.  Enable with -DSNAP_BUILD_LAPG=ON.

add_executable(txt2cplusplus src/txt2c++.cpp)

set(_lapg_src ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(_lapg_templates
    ${_lapg_src}/default_c
    ${_lapg_src}/default_cpp
    ${_lapg_src}/default_cs
    ${_lapg_src}/default_java
    ${_lapg_src}/default_js)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/defaults.cpp
    COMMAND $<TARGET_FILE:txt2cplusplus>
        ${CMAKE_CURRENT_BINARY_DIR}/defaults.cpp
        default_c default_cpp default_cs default_java default_js
    DEPENDS txt2cplusplus ${_lapg_templates}
    WORKING_DIRECTORY ${_lapg_src}
    COMMENT "Generating defaults.cpp")

add_executable(lapg
    src/common.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/defaults.cpp
    src/engine.cpp
    src/gbuild.cpp
    src/lalr1.cpp
    src/lapg.cpp
    src/lbuild.cpp
    src/parse.cpp
    src/srcgen.cpp
    src/templates.cpp
)

if(NOT MSVC)
    target_compile_options(lapg PRIVATE
        -DGCC_COMPILER
        -fpermissive
        -Wno-write-strings
        -Wno-format-security)
endif()

set_target_properties(lapg PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON)
