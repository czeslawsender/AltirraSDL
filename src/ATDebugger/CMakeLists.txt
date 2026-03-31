add_library(ATDebugger STATIC
    source/breakpointsimpl.cpp
    source/debugdevice.cpp
    source/defsymbols.cpp
    source/historytree.cpp
    source/historytreebuilder.cpp
    source/stdafx.cpp
    source/symbolio.cpp
    source/symbols.cpp
    source/target.cpp
)

target_include_directories(ATDebugger
    PUBLIC ${ALTIRRA_INCLUDE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/h
)

target_link_libraries(ATDebugger PUBLIC system ATCore ATCPU)
