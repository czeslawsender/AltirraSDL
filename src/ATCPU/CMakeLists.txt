add_library(ATCPU STATIC
    source/co6502.cpp
    source/co6502machine.cpp
    source/co65802.cpp
    source/co6809.cpp
    source/co8048.cpp
    source/co8051.cpp
    source/coz80.cpp
    source/decode6502.cpp
    source/decode65816.cpp
    source/decode6809.cpp
    source/decodez80.cpp
    source/memorymap.cpp
    source/stdafx.cpp
)

target_include_directories(ATCPU
    PUBLIC ${ALTIRRA_INCLUDE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/h
)

target_link_libraries(ATCPU PUBLIC system)
