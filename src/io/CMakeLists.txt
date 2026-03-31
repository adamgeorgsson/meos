add_library(meos_io STATIC
    io.cpp
    IofXml.cpp
    CsvIo.cpp
    HtmlWriter.cpp
)
target_include_directories(meos_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(meos_io PUBLIC meos_domain meos_util)
