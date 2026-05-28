add_library(meos_util STATIC
    string_util.cpp
    time_util.cpp
    TimeStamp.cpp
    meos_util.cpp
    hls.cpp
    meos_version.cpp
    file_util.cpp
    xmlparser.cpp
    csvparser.cpp
    localizer.cpp
)
target_include_directories(meos_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
