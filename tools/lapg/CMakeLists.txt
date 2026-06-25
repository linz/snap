# Builds the lapg parser-generator tool.
# Only required when regenerating src/snap_manager/scriptimp_parse.cpp from
# its .syntax grammar file.  Enable with -DSNAP_BUILD_LAPG=ON.

add_executable(lapg
    src/common.cpp
    src/defaults.cpp
    src/engine.cpp
    src/gbuild.cpp
    src/lalr1.cpp
    src/lapg.cpp
    src/lbuild.cpp
    src/parse.cpp
    src/srcgen.cpp
    src/templates.cpp
)

target_compile_options(lapg PRIVATE
    -DGCC_COMPILER
    -fpermissive
    -Wno-write-strings
    -Wno-format-security)

set_target_properties(lapg PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON)
