add_library(vdjson STATIC
    source/jsonnametable.cpp
    source/jsonoutput.cpp
    source/jsonreader.cpp
    source/jsonvalue.cpp
    source/jsonwriter.cpp
    source/stdafx.cpp
)

target_include_directories(vdjson
    PUBLIC ${ALTIRRA_INCLUDE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/h
)

target_link_libraries(vdjson PUBLIC system)
